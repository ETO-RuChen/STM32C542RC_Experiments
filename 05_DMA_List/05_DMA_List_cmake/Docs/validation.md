# 最终版本验证与复现

平台：NUCLEO-C542RC，芯片 ID=0x44F / Rev Z，ST-LINK V3J17M11，CubeProgrammer 2.23.0，GCC 14.3.1，115200 8N1 / COM12。记录日期：2026-09-16。
APP_PHASE=1～7 均重新编译通过；最终烧录 APP_PHASE=7，下载后已回读校验。历史阶段的上板数据见 [work_plan.md](work_plan.md)。

## 测试结果与边界

| 项目 | 实际证据 |
| --- | --- |
| 同通道异构节点 | P4 的 UART/TIM request、数据宽度与目的地址读回正确，4 节点单次完成 |
| 正常/报警自主循环 | 连续 Start/Max/Done 与 Alarm 日志；starts=1、completions=0、错误 0 |
| 物理按键 | P7 读到 5 次有效物理事件和改链，进入 ALARM；物理双向视觉效果未另行取得明确确认 |
| 返回正常 | 软件 EXTI13 触发相同 ISR，捕获恢复正常日志，starts 保持 1 |
| 连续请求 | 32 次软件 EXTI，32 次接受，两个分支与期望模式一致，starts=1、errors=0 |
| 消抖过滤 | 仅调试暂停时冻结 TIM6，32 次事件接受 1 次、过滤 31 次；测试后恢复冻结寄存器 |
| 初始化错误分类 | GDB 强制 HAL_UART_Init / HAL_TIM_Init / HAL_DMA_Init 返回 HAL_ERROR，分别进入对应初始化错误 |
| DMA 故障现场 | 非法 Timer 节点长度 3 bytes 触发 USEF；CSR=0x1001、CBR1=3、error_codes=2、snapshot_valid=1；复位恢复 |
| 睡眠与 IRQ | 首次附加前 wakeups=0；SysTick TICKINT=0、TIM2 仅 UDE、TIM6 DIER=0、DMA 仅错误 IRQ |

连续 EXTI 测试使用调试断点，真实时间间隔受主机影响。消抖测试冻结的是调试暂停期间的 TIM6 时间，不是自然机械抖动波形；两者均不代替所有节点边界、40 ms 临界间隔和无调试器长期压力测试。

尚未完成：RM0522 的并发取链规则核对、PA5/PA2 示波器联测、最大切换延迟证明、长期/极端竞争测试。`desired_mode` 与链接字正确只能证明请求已提交，应结合后续日志判断实际进入哪个环。

## 正常运行检查

```powershell
cube cmake --preset debug_GCC_NUCLEO-C542RC -DAPP_PHASE=7
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
./Tools/flash_capture.ps1 -Seconds 8
./Tools/capture_serial.ps1 -Seconds 15
./Tools/read_state.ps1
```

烧录脚本会先打开串口，烧录前仍可能捕获旧固件尾部日志；要区分复位前后输出。未使用调试器前的运行区间才适合读取首次 wakeups；GDB 连接/断开本身可让 WFI 返回一次，后续总数不等于周期中断次数。
最终仅使用普通 Sleep，不测试 Stop/Standby 或绝对 CPU 占用率。

物理双向检查：上电正常呼吸，按一次 B1 等待报警，再按一次等恢复呼吸；每次先留约 5 秒观察。该等待值是操作建议，不是保证的最大切换延迟。快速按键以最终接受的请求为准。

## 自动 EXTI 与消抖测试

关闭其他调试器/串口助手，在本工程目录执行：

```powershell
./Tools/read_state.ps1 -Commands 'source Tools/test_relink.gdb'
./Tools/read_state.ps1 -Commands 'source Tools/test_debounce.gdb'
```

脚本使用 EXTI13 的 SWIER1 bit13，进入真实 EXTI/HAL/应用回调。`RELINK_RESULT failed=0` 和 `DEBOUNCE_RESULT failed=0` 为成功；单纯 GDB 退出码不足以判断，read_state 同时检查脚本错误及结果。
测试要求当前 debug GCC / APP_PHASE=7 的 ELF 与板上固件一致，handler 返回指令已做检查；变更优化/生成代码后应重新审查断点位置。测试会改变期望模式并增加计数，不能与用户手动按键同时进行。
消抖测试若中途异常，在 GDB 中恢复 `$freeze_before` 到 `$freeze_reg` 后再继续，或重新断电上电；不要把调试冻结残留当成固件默认配置。

## 故障复现

以下故意触发故障，仅在已烧录最终版本的实验板上执行，结束后必须复位。

```powershell
./Tools/read_state.ps1 -Commands 'set nodes[1].regs[2] = 3'
./Tools/read_state.ps1
cube programmer -c port=SWD sn=002E00233235510F37333439 mode=UR reset=HWrst freq=1000 -rst
```

先确保处于 NORMAL；若在 ALARM，需先恢复正常，否则不会执行 N2。此测试只改 SRAM 描述符，复位重新建图后恢复，不改 Flash。
预期 fault=APP_FAULT_DMA_RUNTIME、ready=0、error_count=1、error_snapshot_valid=1，PC 停在 app_fault 的 WFI。错误 IRQ 之前保存的字段不同于 HAL 复位后的实时 channel 寄存器。
错误时没有自动恢复/重启 DMA，没有通过故障 UART 打印。为避免调试器刚断开时 BKPT 引入附加 HardFault，固件不主动执行断点指令；可在 app_fault 自行设置断点。

## 再生成与移植检查

保留 `main.c` 应用入口、根 CMake 源文件/编译配置/链接器 wrap 扩展、独立 App/BSP/Drivers_App/Config/Tools/Docs。核对生成器仍为 144 MHz TIM 时钟、PA5 AF1、USART2 TX、PC13 高有效以及正确 request 编号。
描述符必须常驻 DMA 可访问 SRAM、32-bit 对齐、整个数组在同一个 64 KB link 窗口。移植到其他 MCU、RAM/cache 配置、HAL 或非 GNU 链接器时需重新核查，不能仅替换芯片名。
