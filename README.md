# STM32C542RC HAL2 实验工程总览

本仓库面向 **NUCLEO-C542RC / STM32C542RCT6**，包含 5 个由 STM32CubeMX2 导出、使用 STM32 HAL2 和 CMake 构建的独立实验。它们不是一个同时运行所有功能的固件：每个目录都能单独配置、编译和烧录，后烧录的实验会覆盖开发板上原来的固件。

如果你第一次接触 HAL2，建议先读 [工程全景与 HAL2 文件结构](Docs/01_工程全景与阅读路线.md)，再按 `01 -> 05` 的顺序看实验。不要从 `stm32c5xx_drivers/` 开始读，那里面是完整的供应商驱动，不是本项目的应用入口。

## 1. 硬件与公共配置

| 项目 | 配置 |
| --- | --- |
| 开发板 | NUCLEO-C542RC |
| MCU | STM32C542RCT6，Arm Cortex-M33 |
| 系统时钟 | 144 MHz |
| 构建系统 | CMake + Ninja |
| 工具链配置 | GNU Tools for STM32 14.3.1 |
| 用户 LED | PA5 |
| 用户按钮 B1 | PC13 |
| ST-LINK 虚拟串口 | USART2，PA2/TX、PA3/RX |
| 串口参数 | 115200 bit/s，8-N-1，无流控 |
| 下载/调试 | 板载 ST-LINK，SWD |

通常只需用一根支持数据传输的 USB 线连接板载 ST-LINK 接口，就能同时获得供电、SWD 和虚拟串口。

## 2. 实验一览

| 编号 | 实验 | 学习重点 | 可观察结果 |
| --- | --- | --- | --- |
| 01 | [LED 与按键](01_LED_Button/README.md) | GPIO、轮询、软件去抖、part driver | 松开 B1 时 LED 慢闪，按住时快闪 |
| 02 | [UART 与 DMA](02_UART/README.md) | `printf`、Receive-to-Idle、DMA 收发、回调 | 输入数据后显示 ASCII/HEX 并 DMA 回显 |
| 03 | [TIM6 定时中断](03_TIM/README.md) | 定时器、NVIC、中断回调、`WFI` | TIM6 每 0.5 s 翻转一次 LED |
| 04 | [STOP1 低功耗](04_LowPower/README.md) | STOP1、EXTI 唤醒、SysTick、时钟恢复 | 休眠后由 B1 唤醒，串口打印次数 |
| 05 | [LPDMA 链表](05_DMA_List/README.md) | linked-list DMA、硬件执行图、运行时改链 | DMA 自主驱动呼吸灯/报警灯和 UART 日志 |

学习顺序不是强制的，但 05 同时使用 DMA、TIM、UART、EXTI、HAL/LL 和直接寄存器访问，最好在掌握 01～04 后再读。

## 3. 仓库布局

```text
STM32C542RC_Experiments/
├── README.md                         本文：仓库总入口
├── Docs/
│   ├── 01_工程全景与阅读路线.md      HAL2 目录、启动链、代码所有权
│   └── 02_构建烧录与操作手册.md      编译、下载、串口和验收步骤
├── 01_LED_Button/
│   ├── README.md                     实验 01 说明
│   ├── 01_LED_Button.ioc2            CubeMX2 配置源
│   └── 01_LED_Button_cmake/          可编译 CMake 工程
├── 02_UART/                          结构同上
├── 03_TIM/                           结构同上
├── 04_LowPower/                      结构同上
└── 05_DMA_List/
    ├── README.md                     实验 05 入门说明
    ├── 05_DMA_List.ioc2
    └── 05_DMA_List_cmake/
        ├── README.md                 DMA 实验完整说明
        └── Docs/                     架构、节点、上板和验证记录
```

每个实验的 `.ioc2` 是硬件配置源，`*_cmake/` 是导出的源码工程。修改引脚、时钟、DMA 请求或外设参数时，优先修改 `.ioc2` 并重新生成；修改应用行为时，主要编辑 `main.c`、`app_*.c`，以及 05 的 `App/`、`BSP/`、`Config/`、`Drivers_App/`。

## 4. 最短构建流程

在仓库根目录打开 PowerShell。以实验 01 为例：

```powershell
cd .\01_LED_Button\01_LED_Button_cmake
cube cmake --preset debug_GCC_NUCLEO-C542RC
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
```

输出通常位于：

```text
01_LED_Button/01_LED_Button_cmake/build/debug_GCC_NUCLEO-C542RC/
├── 01_LED_Button.elf
└── 01_LED_Button.map
```

其他实验只需进入对应的 `*_cmake` 目录。05 默认构建最终阶段 `APP_PHASE=7`；要显式指定可执行：

```powershell
cd .\05_DMA_List\05_DMA_List_cmake
cube cmake --preset debug_GCC_NUCLEO-C542RC -DAPP_PHASE=7
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
```

完整的环境检查、直接 CMake 备用方法、烧录命令和串口设置见 [构建烧录与操作手册](Docs/02_构建烧录与操作手册.md)。

## 5. 一次启动经历了什么

五个工程的主路径基本相同：

```text
上电/复位
  -> startup_stm32c542xx.c: Reset_Handler
  -> SystemInit() 和 C 运行库初始化
  -> main()
  -> mx_system_init()
       -> HAL、NVIC、Cache、时钟和已启用外设初始化
  -> App_xxx_Init() / app_run()
  -> while (1) 或 WFI
```

HAL2 生成代码常把外设句柄设为 `static`，应用不直接访问全局 `huart2`、`htim6`，而是调用 `mx_usart2_uart_gethandle()`、`mx_tim6_gethandle()`。这是与很多旧 HAL 教程最容易混淆的地方之一。

## 6. 从哪里开始读代码

推荐顺序：

1. 先读对应实验的 `README.md`，知道预期现象和关键外设。
2. 打开 `.ioc2`，看 Pinout、Clock、外设和 DMA 配置。
3. 看 `*_cmake/main.c`，只追踪初始化和主循环。
4. 看 `app_*.c`；05 则从 `App/app_main.c` 开始。
5. 再看应用直接使用的 `generated/hal/mx_*.c`，理解句柄、参数和 IRQ 入口。
6. 只有需要确认 HAL 内部行为时，才进入 `stm32c5xx_drivers/hal/`。

主要应用入口如下：

```text
01_LED_Button/01_LED_Button_cmake/main.c
02_UART/02_UART_cmake/app_uart_demo.c
03_TIM/03_TIM_cmake/app_timer_toggle.c
04_LowPower/04_LowPower_cmake/app_low_power_demo.c
05_DMA_List/05_DMA_List_cmake/App/app_main.c
05_DMA_List/05_DMA_List_cmake/Drivers_App/dma_graph.c
```

## 7. 修改代码的基本规则

| 内容 | 建议修改位置 |
| --- | --- |
| 引脚、时钟、外设、DMA、NVIC | `.ioc2`，然后用 CubeMX2 重新生成 |
| 主循环和业务状态机 | `main.c`、`app_*.c`、05 的 `App/` |
| 板级 LED/按键/串口封装 | 05 的 `BSP/` |
| 应用常量 | 05 的 `Config/` 或应用源文件中的宏 |
| 新增应用源文件 | 根 `CMakeLists.txt` 的 `target_sources()` |
| HAL/CMSIS/芯片包 | 原则上不直接修改，先在应用层封装 |

重新生成前先提交或备份自己的修改，并在生成后检查 `CMakeLists.txt`、`main.c` 和应用文件是否仍被纳入构建。更详细的目录解释和再生成检查清单见 [HAL2 文件结构文档](Docs/01_工程全景与阅读路线.md)。

## 8. 文档导航

- [工程全景与 HAL2 文件结构](Docs/01_工程全景与阅读路线.md)：重点解释 HAL2 为什么没有传统 `Core/Src`，每个目录由谁维护，以及中断/构建调用链。
- [构建烧录与操作手册](Docs/02_构建烧录与操作手册.md)：工具检查、配置、编译、烧录、串口与五个实验的验收步骤。
- [实验 01：LED 与按键](01_LED_Button/README.md)
- [实验 02：UART 与 DMA](02_UART/README.md)
- [实验 03：TIM6 定时中断](03_TIM/README.md)
- [实验 04：STOP1 低功耗](04_LowPower/README.md)
- [实验 05：LPDMA linked-list](05_DMA_List/README.md)

## 9. 关于验证

编译成功只能证明语法、符号、静态配置和链接地址在当前工具链下成立，不能替代开发板验证。LED 引脚、按钮有效电平、串口端口、STOP1 电流以及 DMA 运行时改链的边界，都需要结合原理图、参考手册和上板现象判断。05 已保存专门的验证记录，其余实验的操作验收步骤见各自 README 与总操作手册。
