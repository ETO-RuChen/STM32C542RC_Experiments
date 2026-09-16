# STM32C542RC LPDMA Autonomous Linked-List Demo

目标平台：NUCLEO-C542RC / STM32C542RCT6，CubeMX2 CMake 工程，STM32C5 HAL2。

当前默认版本为 **P3 Timer 静态线性链表验收固件**，已通过上板验证。可通过 `-DAPP_PHASE=1` 或 `2` 重建已验证的早期阶段。
最终单通道异构循环链表及 Runtime Relinking 仍在后续阶段，当前不具备这些功能。

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
cube cmake --preset debug_GCC_NUCLEO-C542RC -DAPP_PHASE=3
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
```

产物：`build/debug_GCC_NUCLEO-C542RC/05_DMA_List.elf`，可由 CubeProgrammer 烧录。
CubeMX2 源配置位于父目录 `../05_DMA_List.ioc2`，应与本目录一起保存。

## 文件导航

- [阶段计划与进度](Docs/work_plan.md)
- [P0 可行性核查](Docs/p0_audit.md)
- [上板操作、预期结果和验收表](Docs/bringup.md)
- [当前软件结构与目标架构](Docs/architecture.md)
- [未来 DMA 节点设计](Docs/dma_nodes.md)

应用入口为 `App/app_main.c`；外设封装位于 `BSP/`；常量位于 `Config/app_config.h`。
应用没有修改 `generated/` 或 HAL/LL 驱动。新增源文件通过根 `CMakeLists.txt` 的扩展位置加入。
`Config/app_hal_overrides.h` 被强制包含到所有翻译单元，在生成配置之后启用参数检查及 DMA/UART 错误记录，保持 HAL handle 布局一致。
重新生成后应检查根 CMake 扩展和 `main.c` 的应用入口仍然保留，再重新构建。

## 调试

观察 `g_app_diagnostics` 和 `g_button_diagnostics`。
应用初始化成功后 `ready=1`、`fault=APP_FAULT_NONE`。
出错时保存错误类型和返回值；连接调试器时进入断点，否则停在 WFI，避免依赖故障串口打印。
字段说明和系统初始化失败排查见 [bringup.md](Docs/bringup.md)。

`Tools/flash_capture.ps1` 可打开串口后烧录并抓取日志；`Tools/read_state.ps1` 可通过 AP1 读取诊断变量。默认探头与 COM12 为本机已验证设备，换板时传入参数。运行前关闭其他占用串口/调试器的程序。
