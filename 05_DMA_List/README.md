# 实验 05：LPDMA Linked-List 与运行时改链

[返回仓库总览](../README.md)

这是仓库中最复杂的实验。默认 `APP_PHASE=7`：LPDMA1_CH0 在不依赖 CPU 逐步调度的情况下，交替执行 UART 日志节点和 TIM2 PWM 波形节点，形成“正常呼吸”和“报警闪烁”两套硬件执行环；B1 按键通过修改预定的 SRAM link word 请求在两套环之间切换。

完整技术说明位于 [05_DMA_List_cmake/README.md](05_DMA_List_cmake/README.md)。本文只提供学习入口和文件导航。

## 1. 建议前置知识

阅读本实验前，至少应理解：

- 实验 01 的 GPIO、按钮和去抖。
- 实验 02 的 UART DMA 与回调。
- 实验 03 的 TIM、PWM/更新事件和 `WFI`。
- 实验 04 的 EXTI、SysTick 与低功耗基本概念。
- C 数组、指针、`volatile`、位字段、内存对齐和链接脚本的基本作用。

## 2. 最终阶段的硬件资源

| 资源 | 配置/用途 |
| --- | --- |
| TIM2_CH1 / PA5 | 1 kHz PWM，DMA 写 CCR1 改变亮度 |
| TIM2 update request | 每 1 ms 请求一个 PWM 样本 |
| USART2 / PA2 | 115200 8-N-1 日志输出 |
| LPDMA1_CH0 | 最终混合 linked-list 图的唯一执行通道 |
| PC13 / EXTI13 | 请求切换正常/报警模式 |
| TIM6 | 10 kHz 单脉冲窗口，40 ms 按键去抖，无 IRQ |
| SysTick | 最终稳定运行前暂停并清 pending |
| CPU | 初始化后主要执行 `WFI`，不逐节点推进动画 |

`LPDMA` 的名字并不自动保证整个 TIM2/USART2 图能在 STOP 模式运行。本实验最终阶段使用普通 Sleep/`WFI`，没有宣称深度低功耗自治。

## 3. 默认现象

正常模式：

```text
约 1 s 渐亮
  -> 约 1 s 渐暗
  -> 约 500 ms 暗态
  -> 循环
```

串口循环输出 Start、LED Max、Cycle Done 等日志。

按 B1 请求报警模式后：

```text
约 75 ms 亮 / 75 ms 灭
  -> 每组 4 次闪烁
  -> 循环输出 [ALARM] Active
```

再次按 B1 请求回到正常模式。请求只改下一跳链接；如果 DMA 已取走旧链接，可能先多执行一轮。`desired_mode` 表示软件期望，不代表硬件在写入瞬间已经进入目标环。

## 4. 硬件执行图

```text
正常环：
N1 UART Start
 -> N2 Fade Up
 -> N3 UART Max
 -> N4 Fade Down
 -> N5 UART Done
 -> N6 Dark Hold / Branch
      -> N1（正常）
      -> A1（请求报警）

报警环：
A1 UART Alarm
 -> A2 Alarm Flash / Branch
      -> A1（继续报警）
      -> N1（请求正常）
```

同一个 DMA channel 在节点边界切换 UART TX request 与 TIM2 update request。HAL Q API 用于启动前建图；运行时只修改 N6/A2 两个预定分支 link，不在 EXTI 回调中执行通用队列插入/删除。

## 5. APP_PHASE

本工程用编译期 `APP_PHASE` 保留逐步 bring-up 路径。一次构建只包含一个阶段：

| 值 | 内容 |
| ---: | --- |
| 1 | 固定 PWM 10%/50%/100%、阻塞 UART、按钮基线 |
| 2 | UART Direct DMA + TIM2_UP Direct DMA |
| 3 | 两个 Timer linked-list 节点 |
| 4 | 单通道 UART/Timer 异构四节点 |
| 5 | 正常六节点自主闭环 |
| 6 | 报警双节点自主闭环 |
| 7 | 正常/报警双环、按钮消抖、运行时改链和诊断 |

P8/P9 是诊断和文档阶段，不是可选的 `APP_PHASE` 值。

## 6. 软件结构

| 路径 | 职责 |
| --- | --- |
| `05_DMA_List.ioc2` | CubeMX2 硬件配置源 |
| `05_DMA_List_cmake/main.c` | 系统初始化，失败转诊断，成功调用 `app_run()` |
| `App/app_main.c` | 各阶段顶层流程、按钮模式请求和 WFI |
| `App/app_diagnostics.c` | 初始化/DMA 错误现场与 linker wrapper |
| `App/bringup_dma.c` | 早期 Direct DMA 验证 |
| `BSP/` | LED、按钮、虚拟串口封装 |
| `Config/app_config.h` | PWM、波形、去抖和串口常量 |
| `Config/app_hal_overrides.h` | 强制包含的 HAL 配置覆盖 |
| `Drivers_App/dma_graph.c` | 描述符、队列、启动和运行时分支改链 |
| `Drivers_App/pwm_waveform.c` | 呼吸与报警 PWM LUT |
| `Drivers_App/logger_dma.c` | DMA 使用的不可变日志缓冲 |
| `Tools/` | 烧录抓串口、读状态和 GDB 验证 |
| `05_DMA_List_cmake/Docs/` | 架构、节点、bring-up、验证与边界 |

建议阅读顺序：

1. `Config/app_config.h`，先知道时间和硬件常量。
2. `App/app_main.c`，理解每个 APP_PHASE 的顶层流程。
3. `Drivers_App/dma_graph.h`，看公开状态和 API。
4. `Drivers_App/dma_graph.c`，结合节点文档阅读建图与改链。
5. `BSP/bsp_button.c`，追踪 EXTI 和 TIM6 去抖。
6. 最后再看生成的 TIM2、USART2、DMA 配置和 HAL Q 实现。

## 7. 构建

默认最终阶段：

```powershell
cd .\05_DMA_List\05_DMA_List_cmake
cube cmake --preset debug_GCC_NUCLEO-C542RC -DAPP_PHASE=7
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
```

切换阶段时必须重新运行 configure：

```powershell
cube cmake --preset debug_GCC_NUCLEO-C542RC -DAPP_PHASE=4
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
```

产物为 `05_DMA_List_cmake/build/debug_GCC_NUCLEO-C542RC/05_DMA_List.elf`。

## 8. 文档导航

- [完整工程说明](05_DMA_List_cmake/README.md)
- [软件与硬件执行图](05_DMA_List_cmake/Docs/architecture.md)
- [DMA 节点和改链语义](05_DMA_List_cmake/Docs/dma_nodes.md)
- [上板步骤与预期](05_DMA_List_cmake/Docs/bringup.md)
- [验证记录与已知限制](05_DMA_List_cmake/Docs/validation.md)
- [可行性核查](05_DMA_List_cmake/Docs/p0_audit.md)
- [阶段计划](05_DMA_List_cmake/Docs/work_plan.md)

## 9. 重要边界

- 运行时改链已有有限上板验证，但仍应结合 STM32C542 参考手册核对 DMA 并发取链和极端竞争边界。
- 最终图独占 LPDMA1_CH0；不要在运行期间用普通 UART/TIM DMA API 抢占它。
- 应用故障后保存诊断状态、关闭可屏蔽中断并停在 `WFI`，没有自动恢复；应读取现场后复位。
- `g_app_diagnostics`、`g_dma_graph`、`g_button_diagnostics` 是主要调试观察点。
- 修改 CubeMX2 配置后，必须检查根 `CMakeLists.txt` 中的 force-include、`--wrap`、自定义源文件和 APP_PHASE 仍然存在。

本实验的目录仍遵循仓库通用 HAL2 结构；基础说明见 [工程全景与 HAL2 文件结构](../Docs/01_工程全景与阅读路线.md)。
