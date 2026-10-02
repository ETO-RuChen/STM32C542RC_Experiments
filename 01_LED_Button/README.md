# 实验 01：LED 与按键

[返回仓库总览](../README.md)

本实验用最少的外设演示 HAL2 工程的基本入口：读取 PC13 上的用户按钮，经过应用层软件去抖后，切换 PA5 用户 LED 的闪烁速度。

## 1. 实验目标

- 认识 `mx_system_init()` 和 `main()` 的分工。
- 使用 CubeMX2 生成的 LED/Button part driver。
- 理解 GPIO 轮询与软件去抖。
- 理解“翻转间隔”和“完整亮灭周期”的区别。

## 2. 硬件配置

| 资源 | 配置 | 用途 |
| --- | --- | --- |
| PA5 | GPIO 推挽输出，低速，初始低电平 | 板载用户 LED |
| PC13 | GPIO 输入，无上下拉 | 板载 B1 按钮 |
| 系统时钟 | 144 MHz | CPU/HCLK |
| SysTick | 1 ms HAL timebase | `HAL_Delay()` |

`generated/parts/mx_led.h` 把 `LED_0` 映射到 PA5，`mx_button.h` 把 `BUTTON_0` 映射到 PC13。应用通过部件名控制硬件，不在主循环里硬编码端口。

## 3. 上板现象

| 按钮状态 | LED 翻转间隔 | 完整亮灭周期 |
| --- | ---: | ---: |
| 松开 | 500 ms | 约 1 s |
| 按住且通过去抖 | 100 ms | 约 200 ms |

一次“翻转”只从亮变灭或从灭变亮，所以完整周期是翻转间隔的两倍。

## 4. 程序流程

`main.c` 的流程是：

```text
mx_system_init()
  -> 初始化 HAL、144 MHz 时钟、PA5、PC13 和 part driver 映射
  -> mx_button_0_getobject()
  -> button_init()
  -> led_off()
  -> while (1)
       -> button_get_state()
       -> 连续采样完成约 30 ms 去抖
       -> 根据稳定状态选择 500 ms 或 100 ms
       -> 到时后 led_toggle()
       -> HAL_Delay(10 ms)
```

主循环每 10 ms 运行一次。`app_button_get_debounced_state()` 只有在连续样本保持一致达到设定时间后才更新稳定状态，从而过滤机械触点短时间抖动。

## 5. 关键代码

| 文件 | 重点 |
| --- | --- |
| `01_LED_Button.ioc2` | PA5、PC13、时钟和板级部件配置源 |
| `01_LED_Button_cmake/main.c` | 全部应用流程、去抖和 LED 定时 |
| `01_LED_Button_cmake/CMakeLists.txt` | 设置 `BUTTON_DEBOUNCE=0` |
| `01_LED_Button_cmake/generated/parts/mx_led.h` | `LED_0 -> PA5` 映射 |
| `01_LED_Button_cmake/generated/parts/mx_button.h` | `BUTTON_0 -> PC13` 映射 |
| `01_LED_Button_cmake/generated/hal/mx_gpio_default.c` | 实际 GPIO 初始化 |
| `01_LED_Button_cmake/part_drivers/` | ST 提供的 LED/Button 通用实现 |

`BUTTON_DEBOUNCE=0` 容易误解：它关闭的是 part driver 内部依赖 EXTI 的事件去抖路径，不代表本实验没有去抖。本实验在 `main.c` 中用轮询实现了独立的 30 ms 去抖。

## 6. HAL2 调用关系

```text
应用：app_led_update()
  -> part driver：led_toggle(LED_0)
  -> 生成映射：LED_0_GPIO_PORT / LED_0_PIN
  -> HAL2：HAL_GPIO_TogglePin()
  -> PA5 寄存器
```

按钮读取也遵循相同层次：

```text
app_button_get_debounced_state()
  -> button_get_state(button)
  -> HAL_GPIO_ReadPin(HAL_GPIOC, HAL_GPIO_PIN_13)
```

这种结构把“这是第几个 LED”与“它接在哪个 MCU 引脚”分开，换板时更容易通过配置改变映射。

## 7. 构建

在仓库根目录执行：

```powershell
cd .\01_LED_Button\01_LED_Button_cmake
cube cmake --preset debug_GCC_NUCLEO-C542RC
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
```

产物：

```text
01_LED_Button_cmake/build/debug_GCC_NUCLEO-C542RC/01_LED_Button.elf
```

烧录方法见 [构建烧录与操作手册](../Docs/02_构建烧录与操作手册.md)。

## 8. 验收步骤

1. 烧录后不按 B1，观察 LED 至少 3 秒，应慢速闪烁。
2. 按住 B1 超过 30 ms，LED 应明显加快。
3. 松开 B1，LED 应在短暂去抖后恢复慢闪。
4. 快速轻触按钮时，如果持续时间未通过去抖窗口，状态可能不会改变，这是预期行为。

## 9. 可以尝试的修改

- 修改 `LED_BLINK_NORMAL_MS` 和 `LED_BLINK_PRESSED_MS`，比较翻转间隔。
- 修改 `BUTTON_DEBOUNCE_TIME_MS`，观察去抖与响应速度的权衡。
- 给代码增加“短按切换模式”的状态，而不是“按住时快闪”。
- 用 SysTick 时间戳替代每轮 `HAL_Delay(10)`，练习非阻塞任务。

若要改变 PA5/PC13 的硬件配置，应修改 `01_LED_Button.ioc2` 并重新生成，不要只改 `generated/`。目录职责见 [HAL2 文件结构文档](../Docs/01_工程全景与阅读路线.md)。
