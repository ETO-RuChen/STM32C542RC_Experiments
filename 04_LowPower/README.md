# 实验 04：STOP1 低功耗与按键唤醒

[返回仓库总览](../README.md)

本实验演示 STM32C542 从正常运行进入 STOP1，再由 PC13/EXTI13 中断唤醒。唤醒后恢复系统时钟，通过 USART2 打印唤醒信息，并用 PA5 快闪提示。

## 1. 实验目标

- 理解普通 `WFI` 与 STOP1 的区别。
- 在进入 STOP1 前暂停 SysTick，避免 1 ms tick 立即唤醒。
- 使用 HAL2 的句柄化 EXTI 和回调注册。
- 唤醒后恢复 RCC 配置。
- 把中断处理限制为“记录事件”，在前台打印和执行状态流程。

## 2. 硬件配置

| 资源 | 配置 | 用途 |
| --- | --- | --- |
| PA5 | GPIO 推挽输出 | 运行/唤醒状态 LED |
| PC13 | GPIO 输入，无上下拉 | 板载 B1 |
| EXTI13 | GPIOC，RISING，interrupt mode | STOP1 唤醒源 |
| USART2 | PA2/TX、PA3/RX，115200 8-N-1 | 状态日志 |
| PWR | STOP1 | 低功耗模式控制 |
| 系统时钟 | HSE/PSI，144 MHz | 唤醒后重新配置 |
| SysTick | 正常运行时 1 ms，休眠前暂停 | HAL 时间基准 |

当前 EXTI 触发沿是上升沿。根据 NUCLEO 板上按钮的实际电平，现象可能是“松开按钮时触发”。如果要改为下降沿，应在 `04_LowPower.ioc2` 中修改并同步调整应用对 trigger 的判断。

## 3. 一轮实验的时序

```text
App_LowPowerDemo_Process()
  -> PA5 慢闪 3 组（每次亮/灭各 200 ms）
  -> 打印“3000 ms 后进入 STOP1”
  -> HAL_Delay(3000)
  -> PA5 关闭
  -> 清软件按键标志
  -> 等待 UART 排空 100 ms
  -> 清 previous power mode 标志
  -> HAL_SuspendTick()
  -> HAL_PWR_EnterStopMode(WFI, STOP1)
       CPU 在此等待可用中断
  -> EXTI13 或其他中断唤醒
  -> HAL_ResumeTick()
  -> mx_rcc_init() 恢复 144 MHz 时钟
  -> 读取并清除 previous power mode
  -> 打印模式、唤醒计数和按键标志
  -> PA5 快闪 6 组（每次亮/灭各 80 ms）
  -> 下一轮
```

代码会循环执行，所以每次唤醒和提示结束后，又会准备进入下一次 STOP1。

## 4. EXTI 中断路径

初始化阶段：

```text
mx_system_init()
  -> mx_gpio_default_init()
       -> HAL_EXTI_Init(&hEXTI13, HAL_EXTI_LINE_13)
       -> 配置 GPIOC + RISING
       -> 启用 EXTI13_IRQn

App_LowPowerDemo_Init()
  -> mx_gpio_default_exti13_gethandle()
  -> HAL_EXTI_RegisterTriggerCallback(handle, callback)
```

唤醒阶段：

```text
PC13 上升沿
  -> EXTI13_IRQHandler()                         generated
  -> HAL_EXTI_IRQHandler(&hEXTI13)               HAL2
  -> App_LowPowerDemo_Exti13Callback()           application
       -> g_WakeupKeyPressed = 1
       -> g_WakeupCount++
```

回调不执行 `printf`、`HAL_Delay` 或时钟恢复，因为刚离开 STOP1 时外设时钟状态还需要前台统一处理。

## 5. 为什么要暂停 SysTick

HAL 默认时间基准通常每 1 ms 产生一次 SysTick 中断。如果带着周期 tick 执行 WFI，CPU 可能在进入 STOP1 后很快被 tick 唤醒，表现为“根本没有睡下去”。

本实验进入前调用 `HAL_SuspendTick()`，返回后调用 `HAL_ResumeTick()`。因此：

- STOP1 内 `HAL_GetTick()` 不继续计时。
- 不能依赖 `HAL_Delay()` 在 STOP1 中自动计满后唤醒。
- 唤醒源主要应是配置好的 EXTI13，但调试器或其他 pending IRQ 也可能唤醒 CPU。

## 6. 为什么要恢复 RCC

STOP 模式会停止或切换部分高速时钟。CPU 从中断返回并不表示整个 144 MHz 时钟树自动回到进入前的状态。

`App_LowPowerDemo_Process()` 在唤醒后调用 `mx_rcc_init()`，重新启用 HSE/PSI、设置总线分频并恢复 SYSCLK。若省略这一步：

- UART 波特率可能不正确。
- `HAL_Delay()` 的时间可能异常。
- 依赖外设时钟的功能可能失效。

## 7. 关键文件

| 文件 | 重点 |
| --- | --- |
| `04_LowPower.ioc2` | PWR、EXTI13、USART2、PA5 和时钟配置源 |
| `04_LowPower_cmake/main.c` | 初始化后循环运行低功耗流程 |
| `04_LowPower_cmake/app_low_power_demo.c` | STOP1、回调、时钟恢复和提示 |
| `04_LowPower_cmake/app_low_power_demo.h` | 应用公开接口 |
| `04_LowPower_cmake/generated/hal/mx_gpio_default.c` | PA5、PC13、EXTI13 及 IRQ |
| `04_LowPower_cmake/generated/hal/mx_pwr.c` | PWR 初始化 |
| `04_LowPower_cmake/generated/hal/mx_rcc.c` | 144 MHz 时钟配置和恢复入口 |
| `04_LowPower_cmake/generated/hal/mx_usart2.c` | 日志串口 |
| `04_LowPower_cmake/generated/utilities/` | `printf` 到 UART 的接线 |

## 8. 构建与运行

```powershell
cd .\04_LowPower\04_LowPower_cmake
cube cmake --preset debug_GCC_NUCLEO-C542RC
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
```

串口设置为 115200 8-N-1，无流控。产物位于：

```text
04_LowPower_cmake/build/debug_GCC_NUCLEO-C542RC/04_LowPower.elf
```

完整烧录方式见 [构建烧录与操作手册](../Docs/02_构建烧录与操作手册.md)。

## 9. 预期日志与验收

启动时：

```text
[LOW_POWER] STOP1 wakeup demo start.
[LOW_POWER] PC13 EXTI13 rising edge wakes MCU, PA5 shows status.
```

进入前会打印：

```text
[LOW_POWER] Enter STOP1 after 3000 ms...
```

按下并松开 B1 后，预期看到类似：

```text
[LOW_POWER] Wakeup from STOP1, count=1, key_flag=1.
```

验收步骤：

1. 上电后确认串口横幅和 PA5 慢闪。
2. 等待进入提示，随后 LED 应关闭。
3. 操作 B1；若上升沿发生在松开时，应在松开后看到唤醒。
4. 确认串口模式为 STOP1、计数递增、`key_flag=1`。
5. 确认 PA5 快闪，然后程序进入下一轮。

## 10. 常见问题与边界

### 一进入 STOP1 就唤醒

检查 SysTick 是否暂停、EXTI pending 是否已存在、是否有其他已启用 IRQ，以及调试器是否在影响低功耗行为。

### 按下按钮没有立即唤醒

当前使用 RISING。先试着松开按钮，再确认 PC13 的实际电平和边沿。不要只在 C 文件中改回调判断，应同步修改 `.ioc2`。

### 唤醒后串口乱码

重点检查 `mx_rcc_init()` 是否成功，以及 USART2 内核时钟和终端波特率。

### 这是否证明了最低功耗

没有。本实验验证的是 STOP1 控制流，不是功耗指标。要测电流，还需按 NUCLEO 原理图/用户手册处理 ST-LINK、LED、跳线和测流路径，并使用电流表或功耗分析仪。

目录职责和 HAL2 EXTI 差异见 [HAL2 文件结构文档](../Docs/01_工程全景与阅读路线.md)。
