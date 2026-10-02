# 实验 05：LPDMA Linked-List PWM 演示

[返回仓库总览](../README.md)

本实验使用一个 LPDMA 通道自动执行 USART2 日志和 TIM2 PWM 占空比波形。正常模式显示呼吸灯，报警模式显示快速闪烁；PC13/B1 按键通过修改 SRAM 中的 DMA 后继链接切换两种模式。

详细说明和构建命令见 [05_DMA_List_cmake/README.md](05_DMA_List_cmake/README.md)。

核心文件：

- `App/app_main.c`：启动应用并进入 `WFI`。
- `BSP/bsp_button.c`：按键 EXTI 和 40 ms 消抖。
- `Drivers_App/dma_graph.c`：构建 8 个节点并运行时改链。
- `Drivers_App/pwm_waveform.c`：生成 PWM 占空比波形表。
- `Drivers_App/logger_dma.c`：提供 DMA 串口日志数据。
