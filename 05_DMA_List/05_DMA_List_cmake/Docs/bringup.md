# P1 上板验收

状态：固件已编译、烧录并回读校验。启动日志和初始 PWM 寄存器已检查，物理按键和实际波形仍待验收。

## 连接和烧录

1. 用可传数据的 USB 线连接开发板 ST-LINK USB 与电脑。
2. 在工程目录运行 `cube programmer --list`，确认 ST-LINK 和 VCP 串口出现。
3. 编译：

   ```powershell
   cube cmake --preset debug_GCC_NUCLEO-C542RC
   cube cmake --build --preset debug_GCC_NUCLEO-C542RC
   ```

4. 确认只连接目标板后，可用 CubeProgrammer GUI 打开 ELF 下载，或运行：

   ```powershell
   cube programmer -c port=SWD mode=UR reset=HWrst freq=1000 -w build/debug_GCC_NUCLEO-C542RC/05_DMA_List.elf -v -rst
   ```

5. 打开 ST-LINK VCP 对应 COM 口，设置 115200 / 8 data bits / no parity / 1 stop bit / no flow control。串口打开后再按 RESET，避免错过上电日志。

当前已识别 COM12；多个探头连接时在 `-c` 后加 `sn=<目标探头序列号>`。首次普通连接报告无法获取 core ID，以上 Under Reset / 1 MHz 参数已在本板成功使用。

调试器使用 AP1（DFP 对 STM32C542 的 debug 描述为 `__ap=1`）。本机启动命令：

```powershell
cube stlink-gdbserver -d -g -m 1 -p 61234 --frequency 1000 -cp 'C:/Users/25108/AppData/Local/stm32cube/bundles/programmer/2.23.0/bin'
```

另一个终端用 `cube arm-none-eabi-gdb build/debug_GCC_NUCLEO-C542RC/05_DMA_List.elf`，执行 `target remote localhost:61234` 后读取诊断变量，最后 `detach` 让应用恢复。调试器会短暂停核，不能把此期间现象用作 CPU 自主运行证明。

## 预期行为

上电输出：

```text
[P1] Hardware bring-up: TIM2_CH1=1kHz, USART2=115200 8N1
[P1] USER button selects fixed PWM: 10% -> 50% -> 100% -> 10%
[P1] PWM=10%, CCR1=100
```

每按一次 B1，再输出一条档位日志。用相隔至少 200 ms 的按键先测基本流程：

| 动作 | 占空比 | CCR1 | PA5 波形预期 |
| --- | --- | --- | --- |
| RESET | 10% | 100 | 1 ms 周期，约 100 us 高电平 |
| 第一次按 B1 | 50% | 500 | 1 ms 周期，约 500 us 高电平 |
| 第二次按 B1 | 100% | 1000 | 持续高电平；计数器仍在运行 |
| 第三次按 B1 | 10% | 100 | 回到约 100 us 高电平 |

ARR=999 时 CCR1=999 是 99.9%；这里的 100% 使用 ARR+1。后续 LUT 的峰值仍可按项目规范选 999，并准确标注。
CCR1 preload 生效需等待 update 边界，因此首次启动/按键写入与输出变化间可能差一个 PWM 周期。

## 按键和消抖

- B1 按下产生上升沿；Board pack 2.1.0 / MB2213 B02 为 HIGH active。
- ISR 只记录上升沿、比较 HAL tick、累计事件，不发送串口、不等待。
- 相邻上升沿相隔小于 40 ms 时丢弃后续沿，并更新最后沿时间；每次接受需距最近上升沿至少 40 ms。
- 这是规范允许的时间戳消抖；超过窗口的异常抖动仍可能被视为新事件。若板上测试出现问题，再升级为一次性定时确认。
- 快速重复按键允许在消抖窗口内被过滤。验收重点是正常单次按下只有一个事件、不死锁、不在 ISR 阻塞。
- P1 保留 SysTick；将来若停 tick，必须先给消抖替换有效时基。

## 调试变量

| 字段 | 意义 |
| --- | --- |
| `g_app_diagnostics.ready` | 应用初始化成功后为 1 |
| `g_app_diagnostics.fault` | app_main.h 中的错误分类，正常为 0 |
| `g_app_diagnostics.fault_detail` | HAL 返回值，或 PWM 配置失败时的实际 timer clock |
| `g_app_diagnostics.system_status` | 仅系统初始化失败时记录 mx_system_init 返回值 |
| `g_app_diagnostics.tim_kernel_hz` | 预期 144000000 |
| `g_app_diagnostics.button_events` | ISR 接受的累计事件 |
| `g_app_diagnostics.processed_events` | 主循环处理完的累计事件，最终追平 button_events |
| `g_app_diagnostics.duty_percent` | 当前选择的 10 / 50 / 100 |
| `g_app_diagnostics.uart_messages` | 启动 2 次发送，此后每接受一个事件加 1 |
| `g_button_diagnostics.raw_edges` | 进入回调的选定沿数量 |
| `g_button_diagnostics.accepted_events` | 通过消抖的事件数量 |
| `g_button_diagnostics.rejected_edges` | 被过滤的沿数量 |
| `g_button_diagnostics.last_edge_ms` | 最近选定沿的 HAL tick |

正常应满足 `raw_edges = accepted_events + rejected_edges`（uint32 模数计数）。
调试器逐字段读取可能跨一次中断；需要一致快照时暂停 CPU 后查看。

系统初始化失败仍沿用生成器的返回分类。`SYSTEM_PERIPHERAL_ERROR` 聚合了 UART/TIM/DMA 初始化错误，需在 `mx_usart2_uart_init`、`mx_tim2_init` 的返回处设断点定位；P8 将进一步完善细粒度初始化诊断。
错误后进入有上下文的停止状态，调试连接存在时触发 BKPT；恢复需复位。

## 验收记录

| 检查 | 预期 | 实测 |
| --- | --- | --- |
| VCP 上电日志 | 三行完整，无乱码 | 已收到完整三行 |
| PA5 10% | 1 kHz / 100 us 高 | 待测 |
| PA5 50% | 1 kHz / 500 us 高 | 待测 |
| PA5 100% | 高电平，TIM2_CNT 持续计数 | 待测 |
| 正常按键 20 次 | 每次一个事件，档位顺序正确 | 待测 |
| 长按/释放 | 无持续重复档位切换 | 待测 |
| 快速按键 | 窗口内可过滤，无卡死 | 待测 |
| TIM2 IRQ | 未启用 update CPU 中断 | 已读 DIER=0 |
| 空闲 | WFI，SysTick 仍周期唤醒 | 调试器捕获 PC 在 WFI |
| 故障记录 | fault=0，ready=1 | 已确认 |

P1 通过后再进入 P2 Direct DMA。P1 代码中的固定 duty 操作和阻塞 UART 仅用于本阶段验收，不是最终自主动画/日志调度实现。
