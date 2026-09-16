# STM32C542RC LPDMA Autonomous Linked-List Demo

目标平台：NUCLEO-C542RC / STM32C542RCT6，CubeMX2 CMake 工程，STM32C5 HAL2。

默认 `APP_PHASE=7`：一个 LPDMA1_CH0 自动执行 UART 日志、PWM 渐变、暗态等待和报警闪烁。PC13/B1 按键在运行时修改 N6/A2 的后继链接，在正常呼吸与报警之间切换；DMA 不停止、不重建、不重启。

已完成上板运行、按键/软件 EXTI 改链、消抖和故障注入验证。RM0522 尚未取得，运行中取链规则、极端竞争边界及最大切换延迟仍待核对；本项目是有实测记录的演示实验，不宣称这些边界已获手册保证。

## 默认演示

- 上电：渐亮约 1 秒 → 渐暗约 1 秒 → 暗态约 500 ms，循环输出 Start / LED Max / Cycle Done。
- 按 B1：请求报警，约 75 ms 亮 / 75 ms 暗，每组四次，循环输出 `[ALARM] Active`。
- 再按 B1：请求恢复正常呼吸。切换在 DMA 重新获取分支链接时生效，可能多运行一轮；`desired_mode` 只表示期望。
- TIM2_CH1/PA5 为 1 kHz PWM，USART2/PA2 经 ST-LINK VCP 输出 115200 8N1 日志。
- 稳态 CPU 执行 WFI，SysTick 中断关闭，无 Timer/节点完成中断推进动画。TIM6 单脉冲计数用于 40 ms 按键消抖，不产生 IRQ。

## 可复现阶段

| APP_PHASE | 行为 |
| --- | --- |
| 1 | 固定 PWM 10% / 50% / 100%，UART、物理按键验证 |
| 2 | Direct DMA，2000 样本单次渐亮/暗，UART DMA 输出 PASS |
| 3 | 两个 Timer 静态链表节点，单次完成 |
| 4 | 同一通道 UART → TIM → UART → TIM，单次完成 |
| 5 | 正常六节点自主循环 |
| 6 | 报警双节点自主循环 |
| 7 | 最终双环及按键改链，包含 P8 诊断完善 |

P8、P9 是诊断和交付阶段，不是额外的 APP_PHASE 值。

## P1 行为（APP_PHASE=1）

- 上电启动 PA5 / TIM2_CH1，1 kHz、10% 固定占空比。
- USART2 / PA2 经 ST-LINK VCP 输出 P1 启动信息，115200、8N1、无流控。
- 每次有效按下 PC13 / B1，依次切换 50%、100%、10%，同时输出当前档位。
- EXTI 回调只记录事件并做 40 ms 时间戳消抖；串口发送在主循环中进行。
- 空闲执行 WFI。P1 保留 SysTick，用于 HAL UART 超时和消抖，不能宣称无周期唤醒。
- P1 不启动 DMA；现有 CubeMX2 的两个 Direct DMA 配置保留供下一阶段验证。

## P2 行为（APP_PHASE=2）

上电由 USART2 DMA 输出开始日志，然后 TIM2_UP Direct DMA 搬运 2000 个 word 样本，在 2 秒内渐亮/渐暗一次。检查完成耗时、剩余字节和最终 CCR1 后再通过 UART DMA 输出 PASS，随后 WFI。只完成单次实验，不自动重启 DMA。

## 构建

在工程根目录执行（本机 STM32Cube 工具入口为 `cube`）：

```powershell
cube cmake --preset debug_GCC_NUCLEO-C542RC -DAPP_PHASE=7
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
./Tools/flash_capture.ps1 -Port COM12 -Seconds 8
```

产物：`build/debug_GCC_NUCLEO-C542RC/05_DMA_List.elf`，可由 CubeProgrammer 烧录。
CubeMX2 源配置位于父目录 `../05_DMA_List.ioc2`，应与本目录一起保存。

## 文件导航

- [阶段计划与进度](Docs/work_plan.md)
- [P0 可行性核查](Docs/p0_audit.md)
- [上板操作、预期结果和验收表](Docs/bringup.md)
- [软件结构与运行架构](Docs/architecture.md)
- [DMA 节点与改链语义](Docs/dma_nodes.md)
- [最终验证方法及限制](Docs/validation.md)

应用入口为 `App/app_main.c`；外设封装位于 `BSP/`；常量位于 `Config/app_config.h`。
应用没有修改 `generated/` 或 HAL/LL 驱动。新增源文件通过根 `CMakeLists.txt` 的扩展位置加入。
`Config/app_hal_overrides.h` 被强制包含到所有翻译单元，在生成配置之后启用参数检查及 DMA/UART 错误记录，保持 HAL handle 布局一致。
初始化错误分类及 DMA 错误清除前的快照通过 GNU ld `--wrap` 接入，见 `App/app_diagnostics.c`。当前支持已有 GCC preset；移植到其他链接器需提供等效入口拦截。
重新生成后应检查根 CMake 扩展（含 force-include 和 wrap）、`main.c` 的应用入口仍然保留，并核对 TIM2、USART2、EXTI 和 DMA 配置，再重新构建。

## 调试

观察 `g_app_diagnostics`、`g_dma_graph` 和 `g_button_diagnostics`。
应用初始化成功后 `ready=1`、`fault=APP_FAULT_NONE`。
出错时保存错误类型、返回值和 DMA 现场并停在 WFI；可在 `app_fault` 手动设置断点，避免依赖故障串口打印。
字段说明和系统初始化失败排查见 [bringup.md](Docs/bringup.md)。

`Tools/flash_capture.ps1` 可打开串口后烧录并抓取日志；`Tools/read_state.ps1` 可通过 AP1 读取诊断变量。默认探头与 COM12 为本机已验证设备，换板时传入参数。运行前关闭其他占用串口/调试器的程序。

GitHub 分支：[feat/05-lpdma-demo](https://github.com/ETO-RuChen/STM32C542RC_Experiments/tree/feat/05-lpdma-demo/05_DMA_List/05_DMA_List_cmake)。build 产物不提交。
