# 实验 03：TIM6 定时中断

[返回仓库总览](../README.md)

本实验让 TIM6 每 0.5 秒产生一次更新中断，在 HAL2 回调中翻转 PA5 用户 LED。主循环没有前台任务，使用 Cortex-M33 的 `__WFI()` 等待下一次中断。

## 1. 实验目标

- 根据定时器内核时钟、PSC 和 ARR 计算更新频率。
- 理解 IRQ 入口、HAL IRQ handler 与应用回调的完整路径。
- 理解 HAL2 定时器句柄 getter。
- 使用 `WFI` 避免无意义空转。

## 2. 硬件与定时参数

| 资源 | 配置 | 用途 |
| --- | --- | --- |
| PA5 | GPIO 推挽输出 | 板载用户 LED |
| TIM6 | Basic timer，向上计数 | 产生周期更新事件 |
| TIM6 kernel clock | 144 MHz | 定时器输入时钟 |
| Prescaler | 1439 | 除以 1440 |
| Auto-reload/period | 49999（`0xC34F`） | 计 50000 个 tick |
| TIM6 IRQ | NVIC priority 0 | 更新中断 |

计算过程：

```text
计数频率 = 144,000,000 / (1439 + 1)
         = 100,000 Hz

更新频率 = 100,000 / (49999 + 1)
         = 2 Hz

更新时间 = 1 / 2
         = 0.5 s
```

每个更新事件只翻转一次 LED，所以一次完整的亮灭周期需要两个事件，约 1 秒。

## 3. 程序流程

```text
main()
  -> mx_system_init()
       -> 配置 PA5
       -> mx_tim6_init()
            -> HAL_TIM_Init()
            -> 设置 PSC/period
            -> 启用 TIM6_IRQn
  -> App_TimerToggle_Init()
       -> mx_tim6_gethandle()
       -> HAL_TIM_Start_IT()
  -> while (1)
       -> __WFI()
```

中断路径：

```text
TIM6 更新事件
  -> Cortex-M33 NVIC
  -> TIM6_IRQHandler()                  generated/hal/mx_tim6.c
  -> HAL_TIM_IRQHandler(&hTIM6)         HAL2 driver
  -> HAL_TIM_UpdateCallback(htim)       app_timer_toggle.c
       -> 检查句柄确实是 TIM6
       -> g_Tim6UpdateCount++
       -> HAL_GPIO_TogglePin(PA5)
  -> 中断返回
  -> 主循环再次执行 WFI
```

`TIM6_IRQHandler()` 由生成层拥有，应用只覆盖 HAL 提供的更新回调。这样外设中断的清标志、状态处理留在 HAL，应用只处理“更新事件发生”这一语义。

## 4. `__WFI()` 是什么

`__WFI()` 是 CMSIS 提供的 Wait For Interrupt 内核指令。执行后 CPU 停止继续取指，直到有可响应中断到来。

本实验中的 `WFI`：

- 不会停止 TIM6，因此定时器仍能按时唤醒 CPU。
- 不等同于 STOP1 等深度低功耗模式。
- 中断处理结束后，程序从 `WFI` 后继续，然后下一轮再次等待。
- 能减少空循环占用，但没有测量和声明具体功耗。

实验 04 才会显式配置并进入 STOP1。

## 5. 关键文件

| 文件 | 重点 |
| --- | --- |
| `03_TIM.ioc2` | TIM6、PA5、时钟和 NVIC 配置源 |
| `03_TIM_cmake/main.c` | 初始化后在主循环执行 `__WFI()` |
| `03_TIM_cmake/app_timer_toggle.c` | 启动 TIM6 IT、更新回调和计数 |
| `03_TIM_cmake/app_timer_toggle.h` | 应用公开接口 |
| `03_TIM_cmake/generated/hal/mx_tim6.c` | PSC、period、IRQ 和静态句柄 |
| `03_TIM_cmake/generated/hal/mx_gpio_default.c` | PA5 GPIO 初始化 |
| `03_TIM_cmake/generated/hal/mx_rcc.c` | 144 MHz 时钟配置 |
| `03_TIM_cmake/CMakeLists.txt` | 将应用文件加入固件 |

HAL2 生成代码把句柄定义为 `static hal_tim_handle_t hTIM6`。应用调用 `mx_tim6_gethandle()` 获取指针，不应另外定义一个“同名句柄”假装它代表同一外设。

## 6. 构建

```powershell
cd .\03_TIM\03_TIM_cmake
cube cmake --preset debug_GCC_NUCLEO-C542RC
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
```

产物位于 `03_TIM_cmake/build/debug_GCC_NUCLEO-C542RC/03_TIM.elf`。烧录方法见 [构建烧录与操作手册](../Docs/02_构建烧录与操作手册.md)。

## 7. 验收步骤

1. 烧录后观察 PA5 LED 至少 5 秒。
2. LED 应每 0.5 秒切换一次亮灭，完整周期约 1 秒。
3. 在 `HAL_TIM_UpdateCallback()` 设置断点，确认 `htim == g_pTim6Handle`。
4. 在调试器中观察 `g_Tim6UpdateCount`，每次回调应加 1。

断点和单步会暂停 CPU，可能改变肉眼观察到的时序；这不是 PSC/ARR 自行变化。

## 8. 常见问题

### LED 不闪

- 确认 `App_TimerToggle_Init()` 没有返回错误。
- 确认 `HAL_TIM_Start_IT()` 返回 `HAL_OK`。
- 检查 `TIM6_IRQn` 已在生成代码中启用。
- 检查 PA5 仍配置为输出，未被其他外设复用。

### 闪烁频率不对

- 先确认 TIM6 实际内核时钟，不要只根据 CPU 主频猜测。
- 计算时 PSC 和 ARR 都要加 1。
- 一个 LED 完整周期包含两次 toggle。
- 修改时钟或 TIM6 参数后，应同步更新注释和本文。

## 9. 可以继续练习

- 将更新频率改为 10 Hz，并重新计算 PSC/ARR。
- 在回调中只置标志，把 GPIO 翻转移到主循环，对比两种结构。
- 用另一个定时器输出 PWM，理解 basic timer 和 channel timer 的区别。
- 用逻辑分析仪测 PA5 实际周期，比较理论值和测量值。

HAL2 目录和中断层次的进一步说明见 [HAL2 文件结构文档](../Docs/01_工程全景与阅读路线.md)。
