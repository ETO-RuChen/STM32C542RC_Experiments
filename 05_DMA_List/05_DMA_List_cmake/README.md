# STM32C542RC DMA 链表 PWM 演示

本工程固定实现最终 DMA 链表方案：一个 `LPDMA1_CH0` 自动执行 USART2 日志和 TIM2 PWM 占空比波形，CPU 不逐节点参与调度。PC13/B1 按键在运行时修改两个 SRAM 链接字，在正常呼吸和报警闪烁之间切换。

## 运行效果

- 正常模式：约 1 秒渐亮、约 1 秒渐暗、约 500 ms 暗态，循环发送 `Start`、`LED Max`、`Cycle Done` 日志。
- 报警模式：约 75 ms 亮、约 75 ms 灭，连续闪烁 4 次，循环发送 `[ALARM] Active`。
- TIM2_CH1/PA5 输出 1 kHz PWM；USART2/PA2 通过 ST-LINK VCP 输出 115200 8N1 日志。
- 稳态 CPU 执行 `WFI`；TIM6 单脉冲窗口完成 40 ms 按键消抖，不产生 TIM6 中断。

## 硬件链表

```text
正常环：N1 UART Start -> N2 Fade Up -> N3 UART Max
      -> N4 Fade Down -> N5 UART Done -> N6 Dark Hold
      -> N1 或 A1

报警环：A1 UART Alarm -> A2 Alarm Flash -> A1 或 N1
```

所有节点由同一个 LPDMA 通道执行。HAL Q 只在启动前建立链表；运行期间按键只更新 N6/A2 的后继 CLLR，不停止、不重建 DMA。

## 构建

在本目录执行：

```powershell
cube cmake --preset debug_GCC_NUCLEO-C542RC
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
```

产物为 `build/debug_GCC_NUCLEO-C542RC/05_DMA_List.elf`。CubeMX2 源配置位于父目录 `../05_DMA_List.ioc2`。

## 代码导航

| 路径 | 职责 |
| --- | --- |
| `App/app_main.c` | 初始化检查、启动 DMA 图、低功耗等待和按键模式请求 |
| `App/app_diagnostics.c` | 初始化失败分类和 DMA 错误现场保存 |
| `BSP/bsp_button.c` | PC13 EXTI 和 TIM6 按键消抖 |
| `BSP/bsp_led.c` | 启动 TIM2 PWM 输出 |
| `Drivers_App/dma_graph.c` | 8 个 DMA 节点、队列和运行时改链 |
| `Drivers_App/pwm_waveform.c` | 呼吸灯和报警闪烁查找表 |
| `Drivers_App/logger_dma.c` | DMA 使用的日志字符串 |
| `Config/app_config.h` | 时钟、PWM、波形和消抖参数 |

生成的 HAL、CMSIS、启动文件和链接文件位于 `generated/`、`arch/`、`stm32c5xx_dfp/`、`stm32c5xx_drivers/`、`utilities/` 和 `user_modifiable/`，不要手动删除。

重新生成 CubeMX2 文件后，应检查根 `CMakeLists.txt` 中的强制包含、GNU `--wrap` 配置和自定义源文件，并重新构建。
