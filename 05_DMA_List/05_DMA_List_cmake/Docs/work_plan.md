# 实施计划与进度

根据用户提供的 `AGENTS_STM32C542_LPDMA_PROJECT.md` 及确认的 P0～P9 计划执行。
每阶段区分源码/编译检查与上板实测，不用编译结果替代硬件验收。

| 阶段 | 目标 | 当前状态 / 下一步 |
| --- | --- | --- |
| P0 | HAL2、request、节点内存、运行时重连规则核查 | 已核查本地 HAL/LL、板包、链接脚本；RM0522 的运行中重连/取链时序仍待核实 |
| P1 | 固定 PWM、UART、PC13 EXTI、消抖 | 上电 UART/寄存器通过；用户确认三档亮度及长按行为；进入 P2，示波器定量波形留待补测 |
| P2 | TIM2_UP Direct DMA 和 USART2 DMA TX | 已上板通过：2000 ms、remaining=0、CCR1=0、UART 完成 2 次、错误 0 |
| P3 | 同类型静态线性 Linked List | 已上板通过：两个 Timer 节点 2000 ms、启动 1 次、完成 1 次、错误 0 |
| P4 | 单通道 UART → TIM → UART → TIM | 已上板通过：4 节点、starts=1、completions=1、错误 0，UART 文本顺序正确 |
| P5 | N1～N6 正常循环 | 已实测连续日志；starts=1、completion IRQ=0、sleep_wakeups=0，SysTick 中断关闭 |
| P6 | A1～A2 报警循环 | 已上板验证重复 Alarm 日志；starts=1、wakeups=0、错误 0；LUT 75 ms 亮/75 ms 暗，共四次 |
| P7 | PC13 安全点 Runtime Relinking | 已实现并上板改链；5 次物理按键，软件 EXTI 返回正常，starts=1、错误 0；RM 规则和压力边界仍待核查 |
| P8 | 错误诊断、IRQ 和 CPU 睡眠收敛 | 已完成初始化分类、清除前 DMA 快照及故障注入；SysTick 停、仅必要 IRQ；P1～P7 回归编译通过 |
| P9 | 系统验收和文档完善 | 文档、复现脚本和有限上板验收已交付；保留 RM、物理双向确认、示波器和长测项 |

## 本次执行记录（2026-09-16）

- 新增 App、BSP、Config 模块，并接入应用入口及 CMake。
- 核查 HAL2 的 PWM 启动、UART 发送、EXTI 配置/注册/使能 API。
- Board pack 2.1.0 / MB2213 B02 声明 B1、LD1 均为高有效。
- `cube cmake --preset debug_GCC_NUCLEO-C542RC` 成功。
- `cube cmake --build --preset debug_GCC_NUCLEO-C542RC` 成功，33 个构建步骤，无编译警告。
- ELF 大小：text 25496 bytes、data 96 bytes、bss 2192 bytes（工具输出值）。
- 初次 CubeProgrammer 2.23.0 枚举无设备；用户连接后识别到 NUCLEO-C542RC、COM12。
- ST-LINK V3J17M11，SN `002E00233235510F37333439`；目标 ID=0x44F、Rev Z、3.30 V。
- 普通连接首次失败；使用 Under Reset / Hardware Reset / SWD 1000 kHz 后烧录和回读校验成功，仅擦写 ELF 覆盖的扇区 0～3。
- COM12 收到完整 P1 banner 和 PWM=10% / CCR1=100 日志。
- 通过 ST-LINK GDB server 的 AP1 读取：ready=1、fault=NONE、tim_kernel_hz=144000000、duty_percent=10、uart_messages=2。
- TIM2 寄存器：CR1=1、DIER=0、PSC=143、ARR=999、CCR1=100、CCER=1；PC 位于 WFI。
- 此次读取 button_events/raw_edges 均为 0；尚未采集 PA5 波形，未验证物理按键三个档位。
- 后续用户回答“是”，确认三档亮度及长按/释放验收；P1 功能闸门通过，定量 PA5 波形仍未采集。

## 仍需补充的证据

RM0522 未取得；运行时改链目前作为硬件实验实现，不能以实测代替手册对并发取链规则的保证。
PA5 定量波形、极端按键边界和长期稳定性应单独补测。

## P2 实测

`APP_PHASE=2`：通过 CubeProgrammer 烧录/校验，收到两条完整 DMA 日志：

```text
[P2] USART2 DMA TX OK; TIM2_UP DMA: 2000 samples / 2000ms
[P2] PASS: TIM2 DMA complete, CCR1=0, UART DMA complete
```

运行结束后 GDB/AP1 读取 `g_bringup_dma`：timer_completions=1、uart_completions=2、elapsed_ms=2000、remaining_bytes=0、final_ccr=0、dma_errors=0、uart_errors=0、passed=1。
`g_app_diagnostics.fault=NONE`、ready=1，PC 位于 bringup_direct_run 的 WFI。
LUT 只在初始化填充一次；运行过程中未由 CPU 更新 CCR1。计时和完成 IRQ 仅用于单次 bring-up 验收。

GitHub：沿用现有仓库 `ETO-RuChen/STM32C542RC_Experiments`，按阶段推送至 `feat/05-lpdma-demo`；不提交 build 产物。

## P3 实测

`APP_PHASE=3`：启用项目级 linked-list 配置，两个静态 word 节点通过 HAL_Q 连接在 LPDMA1_CH0 上。
串口收到 `[P3] Timer linked list: UP -> DOWN` 与 `[P3] PASS: two timer nodes completed autonomously`。
GDB：node_count=2、starts=1、completions=1、elapsed_ms=2000、error_count=0；两个节点各 4000 bytes，尾节点 CLLR=0，最终 CCR1/CBR1=0。
整个 8 节点预留数组位于 0x2000020C～0x200002EB，32-bit 对齐且不跨 64 KB 窗口。
CPU 在链表完成后检查结果，没有通过节点 callback 推进后继。

## P4 实测

`APP_PHASE=4` 使用同一 LPDMA1_CH0 执行 N1 UART、N2 Timer、N3 UART、N4 Timer；LPDMA2 不启动。
串口顺序：P4 banner → `[NORMAL] Cycle Start` → `[NORMAL] LED Max` → P4 PASS。
GDB：node_count=4、starts=1、completions=1、elapsed_ms=2000、error_count=0。
节点读回：UART CTR1=0x8、CTR2=0xC000C00F、目的 TDR=0x40004428；Timer CTR1=0x2000A、CTR2=0xC000C023、目的 CCR1=0x40000034。证明 request、宽度、源/目的与块长度随链表切换。
计时为 HAL 毫秒分辨率；不能由此断言每个样本边界都严格无相位误差，UART 节点期间 TIM2 持续运行，request 边界行为仍需结合 RM 和波形分析。

## P5 实测

`APP_PHASE=5`：N1～N6 环包含固定源 zero 的 2000-byte/500 次 update 暗态节点。9 秒捕获中出现四轮 Cycle Start，前三轮 Start/Max/Done 完整有序。
GDB：node_count=6、starts=1、completions=0、error_count=0、ready=1、sleep_wakeups=0。
SysTick CTRL=0x00010005，TICKINT=0；DMA 仅使能 DTE/ULE/USE 错误中断，TIM2 DIER 只使能 UDE。
调试器断开恢复后再次读取 wakeups=1；这一次来自调试器使 WFI 返回，不能当作周期中断。首次未附加调试器的 9 秒运行中 wakeups=0。

## P6 实测

`APP_PHASE=6`：正常环和报警环的 8 个节点共用静态 SRAM 数组，启动 A1→A2→A1。
5 秒捕获到 9 条 `[ALARM] Active`，GDB 显示 starts=1、completions=0、error_count=0、sleep_wakeups=0。
A2 CBR1=0x960（600 samples / 2400 bytes），每 75 个 update 切换一次亮/暗平台，每轮四次亮灭。

## P7 实测

`APP_PHASE=7` 上电进入正常环。按键 ISR 只更新 N6/A2 的对齐 SRAM CLLR 字；先建立目标环再开放另一环的出口，未调用 stop/restart 或运行中 HAL_Q 操作。
TIM6 使用 10 kHz、400 ticks 的 40 ms 单脉冲计数，无 IRQ/DMA；每次上升沿重启窗口，窗口内沿被拒绝。SysTick 中断关闭。

先捕获 45 秒正常日志；之后读到物理按键 raw_edges=accepted_events=5、relinks=5、desired_mode=ALARM、starts=1、completions=0、error_count=0。
8 秒串口捕获到 13 条 Alarm。经 EXTI13 软件上升沿（SWIER1 bit13）触发相同 ISR 后，6 秒捕获恢复完整 Start/Max/Done 正常序列。
这是物理按键进入报警及软件 EXTI 返回正常的证据；尚不把物理双向视觉效果或所有竞争边界标为已验收。
调试器连接会唤醒 WFI，因此此时 wakeups 计数包含调试影响。

## P8 实测

- GNU ld `--wrap` 在独立 `app_diagnostics.c` 拦截生成器初始化调用和 HAL DMA IRQ 入口，未改生成代码。UART/TIM 初始化返回 NULL 可区分；DMA Init/Direct Config 的底层错误优先保留。
- 分别用 GDB 在 HAL_UART_Init、HAL_TIM_Init、HAL_DMA_Init 强制返回 HAL_ERROR，确认进入 APP_FAULT_UART_INIT、APP_FAULT_TIM_INIT、APP_FAULT_DMA_INIT。其余生成器失败仍保留 system_status 分类。
- `test_relink.gdb` 注入 32 次 EXTI，上板得到 accepted=32、rejected=0、starts=1、errors=0，两个分支始终匹配期望模式。调试暂停改变真实请求间隔，不能据此宣称无干扰极限压力验收。
- `test_debounce.gdb` 仅在调试暂停时冻结 TIM6，排除主机暂停时长：32 次事件中 accepted=1、rejected=31；DMA/TIM2 未冻结，错误 0。测试结束恢复原 DBGMCU 冻结配置。
- 在正常环将 N2 的 SRAM 描述符 CBR1 改为非法 3 bytes，触发 USEF。记录 error_count=1、error_codes=2、error_snapshot_valid=1、CSR=0x1001、CBR1=3、CTR1=0x2000A、目标=0x40000034，ready=0、fault=DMA_RUNTIME。复位恢复有效图。
- 调试器断开后，显式 BKPT 可能引发附加 HardFault；故障停止改为保存现场后 WFI，调试时可自行在 app_fault 设断点。
- 首次附加前读到 wakeups=0；SysTick CTRL=0x10005、TIM2 DIER=0x100、TIM6 DIER=0、LPDMA CCR=0xC01C01。TIM6 消抖无需 IRQ，TIM2 仅 UDE，DMA 仅 DTE/ULE/USE。
- 移除 BKPT 后重新烧录并重复 USEF 故障注入，PC 保持 app_fault/WFI、现场完整；复位后重新建图恢复正常。
- APP_PHASE=1～7 全部回归编译成功，无编译警告，最终返回 7。最终固件连续捕获 45 秒正常日志后首次附加，ready=1、wakeups=0、starts=1、completions=0、errors=0；TIM6 CR1=8（已自动停止）、LPDMA2 CCR=0xC00000（EN=0）。

## P9 交付

README、architecture、dma_nodes、bringup、p0_audit 已与最终实现同步；新增 validation.md，提供 EXTI/消抖/故障测试步骤及证据边界。
最终 ELF：text=29280、data=96、bss=13024 bytes（GNU size 输出，不等同于动态栈峰值）。源码和 CubeMX2 配置按阶段提交至 GitHub `feat/05-lpdma-demo`，build 产物排除。
最终板上为 APP_PHASE=7 正常模式，串口和调试助手结束后释放连接。没有把未取得的 RM 依据、未采集的物理波形或长期压力测试标为已通过。
