# 实验 05：STM32C542RC DMA 链表 PWM 工程详细说明

本文对应当前简化后的 `05_DMA_List` 工程。它不再包含 P1～P6 的逐阶段点火代码，也不再提供独立的 `Tools/` 测试脚本；工程固定实现最终的 DMA linked-list 运行路径。

建议先阅读本文的“快速理解”，再根据“源码阅读路线”打开代码。文中涉及的文件路径均相对于仓库根目录。

## 1. 快速理解

这个工程要完成的事情可以概括为：

```text
TIM2 每 1 ms 产生一次更新请求
        |
        v
LPDMA1_CH0 根据当前 DMA 节点，把数据搬到不同外设
        |
        +--> 把 PWM 占空比样本写入 TIM2->CCR1，控制 PA5 LED 亮度
        |
        +--> 把日志字符串写入 USART2->TDR，输出到 ST-LINK 虚拟串口
        |
        +--> 节点执行完后自动跳到下一个节点

PC13 按键 --> EXTI13 --> 40 ms TIM6 去抖 --> 修改两个 DMA 后继链接
                                      |
                                      +--> 正常呼吸环
                                      +--> 报警闪烁环
```

关键点是：LED 的亮度变化和串口日志的发送都由 DMA 硬件链表推进，CPU 不需要每 1 ms 修改一次 CCR1，也不需要每条日志调用一次发送函数。应用启动完成后，CPU 主要执行 `WFI` 等待中断或按键事件。

## 2. 工程位置和版本边界

实验 05 有两个重要文件：

```text
05_DMA_List/
├── 05_DMA_List.ioc2                 CubeMX2 硬件配置源
├── README.md                         实验入口说明
└── 05_DMA_List_cmake/
    ├── CMakeLists.txt                当前工程的应用扩展和链接配置
    ├── CMakePresets.json              构建 preset
    ├── main.c                         C 入口
    ├── App/                           应用控制和错误诊断
    ├── BSP/                           LED、按键板级封装
    ├── Config/                        应用常量和 HAL 覆盖
    ├── Drivers_App/                   DMA 图、PWM 表和日志数据
    ├── generated/                     CubeMX2 生成的 HAL 初始化代码
    ├── arch/                          CMSIS
    ├── stm32c5xx_dfp/                 STM32C5 设备包
    ├── stm32c5xx_drivers/             STM32 HAL/LL 驱动
    ├── utilities/                     basic_stdio 和 syscalls
    └── user_modifiable/               启动文件、链接脚本等
```

当前版本已经删除：

- `App/bringup_dma.c/.h`：早期 Direct DMA 点火测试；
- `BSP/bsp_vcp.c/.h`：早期阻塞式串口测试接口；
- `Tools/`：烧录抓串口、GDB 验证和调试辅助脚本；
- `05_DMA_List_cmake/Docs/`：与旧 P1～P7 阶段流程绑定的历史文档；
- `APP_PHASE` 编译选项和对应的阶段分支。

这些删除不会改变最终模式的 DMA 链表原理。简化前的完整工程已经保存在 Git 提交 `ec5cae9`，当前简化版本为 `e8ad3a0`。

## 3. 硬件资源

| 资源 | 当前用途 | 关键配置 |
| --- | --- | --- |
| MCU | 运行固件 | STM32C542RCT6，Arm Cortex-M33 |
| 系统时钟 | 提供系统和外设时基 | 144 MHz |
| PA5 / TIM2_CH1 | 输出 LED PWM | PWM1，1 kHz |
| TIM2 更新事件 | 触发 PWM 样本搬运 | 每 1 ms 一次 |
| PA2 / USART2_TX | 输出日志 | 115200、8-N-1、无流控 |
| PC13 / EXTI13 | B1 按键输入 | 上升沿，高电平有效 |
| TIM6 | 按键消抖计时 | 10 kHz 计数，40 ms 单脉冲 |
| LPDMA1_CH0 | 最终混合 DMA 图 | 同时处理 TIM2 和 USART2 请求 |
| LPDMA2_CH0 | CubeMX2 生成的 USART DMA 资源 | 最终应用日志不使用它发送数据 |

### 3.1 TIM2 的频率计算

CubeMX2 生成的 TIM2 配置为：

```text
TIM2 内核时钟 = 144 MHz
PSC           = 143
计数频率      = 144 MHz / (143 + 1) = 1 MHz
ARR           = 999
PWM/更新频率  = 1 MHz / (999 + 1) = 1 kHz
```

因此每一个 TIM2 更新事件间隔 1 ms。DMA 的 Timer 节点每收到一次更新请求，就把一个 32 位值写到 `TIM2->CCR1`。

PWM 占空比的有效计数范围是 `0` 到 `999`：

- `0` 表示关闭输出；
- `999` 表示接近满亮；
- ARR 为 `999`，所以当前波形表的最高值是 `APP_PWM_PERIOD_COUNTS - 1`，即 `999`。

### 3.2 UART 的实际使用方式

CubeMX2 生成的 `mx_usart2.c` 会初始化 USART2，并为 USART2 配置一个 LPDMA2 TX handle。这是生成配置的一部分。

但是，最终应用的日志节点在 `dma_graph.c` 中使用的是：

```c
HAL_LPDMA1_REQUEST_USART2_TX
```

也就是说，最终图使用 LPDMA1_CH0 直接向 `USART2->TDR` 写入日志数据。这样 UART 日志和 TIM2 PWM 才能放进同一个 DMA 链表中，并在节点边界切换请求类型。应用不会在图运行期间调用 `HAL_UART_Transmit_DMA()` 抢占另一个 DMA 路径。

PA3/RX 当前不是应用数据路径；固件只使用 USART2 TX 输出日志。

## 4. 上电后的完整启动流程

### 4.1 从复位到 `app_run`

程序调用关系如下：

```text
复位
  -> startup_stm32c542xx.c
  -> SystemInit() 和 C 运行库初始化
  -> main()
  -> mx_system_init()
       -> HAL_Init()
       -> NVIC 初始化
       -> I-Cache 启动
       -> RCC/时钟树配置
       -> GPIO/EXTI13 初始化
       -> USART2 初始化
       -> TIM2、TIM2 PWM 和 LPDMA1_CH0 初始化
  -> app_run()
```

`main.c` 只做两件事：调用 `mx_system_init()`，检查返回的 `system_status_t`，然后进入 `app_run()`。如果系统初始化失败，程序进入 `app_system_fault()`，不会继续使用不完整的外设句柄。

### 4.2 `app_run` 做什么

`App/app_main.c` 中的 `app_run()` 按以下顺序执行：

1. 读取 TIM2 实际内核时钟。
2. 检查时钟、PSC 和 ARR 是否与 `app_config.h` 中的设计常量一致。
3. 调用 `dma_graph_build()` 生成 PWM 表、DMA 节点和两个队列。
4. 调用 `dma_graph_start()` 启动 USART2、LPDMA1、TIM2 DMA request 和 PA5 PWM。
5. 调用 `bsp_button_start()` 配置 TIM6 去抖和 EXTI13 回调。
6. 设置 `g_app_diagnostics.ready = 1`。
7. 暂停 SysTick 并清除一个可能已经 pending 的 SysTick。
8. 进入永久 `WFI` 循环。

应用不会在主循环中轮询节点，也不会周期性地计算占空比。启动后，正常动画和日志由 DMA 图自行运行。

## 5. DMA 链表的核心原理

### 5.1 什么是这里的“节点”

一个 DMA 节点保存一组完整的传输配置，例如：

- DMA 请求源是 TIM2 更新还是 USART2 TX；
- 源地址和目的地址；
- 源/目的地址是否递增；
- 数据宽度是 byte 还是 word；
- 要搬运多少字节；
- 执行完成后链接到哪个节点。

这样，节点 1 可以是“从 Flash 把字符串写 USART2”，节点 2 可以是“从 SRAM 把 32 位占空比写 TIM2 CCR1”。同一个 LPDMA1_CH0 在节点边界切换完整的传输参数，不需要 CPU 逐次重配 DMA。

### 5.2 两个队列

工程创建两个 HAL Q 队列：

```text
normal_q：正常呼吸环
alarm_q ：报警闪烁环
```

队列只在启动前通过 HAL Q API 构建。DMA 开始运行以后，代码不再调用 HAL Q 的插入、删除或重建接口，避免在 DMA 正在取链时修改整个队列结构。

### 5.3 节点总表

当前静态节点数组为：

```c
enum { N1, N2, N3, N4, N5, N6, A1, A2, NODE_COUNT };
```

对应关系如下：

| 节点 | 类型 | 源数据 | 目的地址 | 数据宽度 | 传输量 | 作用 |
| --- | --- | --- | --- | --- | --- | --- |
| N1 | UART | Flash 中的 `Cycle Start` | `USART2->TDR` | 8 bit | 22 字节 | 正常环开始日志 |
| N2 | TIM | SRAM 中的渐亮表 | `TIM2->CCR1` | 32 bit | 1000 个样本，4000 字节 | 渐亮约 1 秒 |
| N3 | UART | Flash 中的 `LED Max` | `USART2->TDR` | 8 bit | 18 字节 | 达到最高亮度日志 |
| N4 | TIM | SRAM 中的渐暗表 | `TIM2->CCR1` | 32 bit | 1000 个样本，4000 字节 | 渐暗约 1 秒 |
| N5 | UART | Flash 中的 `Cycle Done` | `USART2->TDR` | 8 bit | 21 字节 | 正常环结束日志 |
| N6 | TIM | SRAM 中的 `dark_zero` | `TIM2->CCR1` | 32 bit | 500 个样本，2000 字节 | 全暗保持约 500 ms，并负责分支 |
| A1 | UART | Flash 中的 `Alarm` | `USART2->TDR` | 8 bit | 16 字节 | 报警日志 |
| A2 | TIM | SRAM 中的报警表 | `TIM2->CCR1` | 32 bit | 600 个样本，2400 字节 | 4 组 75/75 ms 闪烁，并负责分支 |

字节数由字符串长度和波形参数计算得出，实际长度以 `logger_dma.c` 和 `pwm_waveform.c` 为准。表格中的日志字节数包含 `\r\n`，不包含 C 字符串结尾的 `NUL`。

### 5.4 正常环和报警环

初始启动的是正常环：

```text
N1 -> N2 -> N3 -> N4 -> N5 -> N6
                              |
                              +--> N1：继续正常模式
                              +--> A1：切换到报警模式
```

报警环是：

```text
A1 -> A2
      |
      +--> A1：继续报警模式
      +--> N1：切回正常模式
```

N6 和 A2 是两个“分支节点”。它们本身仍然负责最后一段 PWM 传输，只是在节点执行结束后决定下一个节点是正常环入口还是报警环入口。

## 6. PWM 波形是如何生成的

`Drivers_App/pwm_waveform.c` 在启动时生成三个 SRAM 数组：

```c
static uint32_t up[UP_COUNT];
static uint32_t down[DOWN_COUNT];
static uint32_t alarm[ALARM_COUNT];
```

### 6.1 渐亮表

```text
UP_COUNT = APP_FADE_UP_MS * APP_PWM_FREQUENCY_HZ / 1000
         = 1000 * 1000 / 1000
         = 1000 个样本
```

每个样本按整数线性插值从 `0` 递增到 `999`。每 1 ms 消费一个样本，所以整个过程约 1 秒。

### 6.2 渐暗表

渐暗表从 `999` 递减到 `0`，同样包含 1000 个样本，耗时约 1 秒。

### 6.3 暗态保持

N6 使用固定源地址 `dark_zero`。DMA 的源地址模式设置为 fixed，因此每次 TIM2 更新都把同一个零值写入 CCR1。

```text
APP_DARK_HOLD_MS * APP_PWM_FREQUENCY_HZ / 1000
= 500 * 1000 / 1000
= 500 次 TIM2 更新
```

这比让 CPU 在 500 ms 内反复写 CCR1 更符合本工程“由 DMA 图自主运行”的目标。

### 6.4 报警表

报警表由全亮和全暗两个平台组成：

```text
每半周期 = 75 ms * 1000 Hz / 1000 = 75 个样本
总样本数 = 2 * 75 * 4 = 600 个样本
```

每 75 个样本在 `999` 和 `0` 之间切换，共 4 次亮灭组，整个 A2 约运行 600 ms。

所有波形数组必须位于 DMA 可访问的 SRAM 中，因此代码没有把它们声明为 `const`。`pwm_waveform_init()` 还会检查边界，出错时进入 `APP_FAULT_WAVEFORM`。

## 7. UART 日志是如何发送的

日志字符串在 `logger_dma.c` 中定义为静态 `const char[]`：

```text
[NORMAL] Cycle Start\r\n
[NORMAL] LED Max\r\n
[NORMAL] Cycle Done\r\n
[ALARM] Active\r\n
```

这些字符串位于 Flash，生命周期贯穿整个程序。每个 UART 节点记录：

```text
源地址       = 字符串首地址
目的地址     = USART2->TDR
源地址模式   = 递增
目的地址模式 = 固定
源/目的宽度  = 8 bit
请求源       = USART2 TX
```

UART 节点的传输完成后，DMA 自动进入下一个 TIM 节点。CPU 不需要等待发送完成，也不需要在每轮循环中重新调用 UART API。

## 8. 按键、去抖和运行时改链

### 8.1 EXTI 配置

CubeMX2 把 PC13 配置为输入并建立 EXTI13。当前工程将 B1 视为高电平有效，因此使用上升沿触发。

生成的 `EXTI13_IRQHandler()` 调用 HAL EXTI handler，HAL 再调用 `bsp_button.c` 注册的 `button_trigger()`。

### 8.2 TIM6 单脉冲去抖

系统进入最终运行状态后会暂停 SysTick，所以不能依靠 `HAL_GetTick()` 做按键时间差判断。工程使用 TIM6：

1. TIM6 计数频率配置为 10 kHz；
2. 自动重载值对应 40 ms；
3. 每次匹配的按键边沿到来时，读取 TIM6 是否仍在计数；
4. 关闭计数器、清零计数器和更新标志，再重新启动一次单脉冲计时；
5. 如果原计数器仍在运行，当前边沿被判定为消抖窗口内的无效边沿；
6. 如果计数器已经停止，则认为这是有效按键，调用应用回调。

TIM6 本身不会开启中断。它只作为一个硬件时间窗口，避免 SysTick 周期性唤醒 CPU。

### 8.3 有效按键回调

`mode_button_event()` 在 EXTI 中断上下文执行：

1. 根据 `desired_mode` 判断目标模式；
2. 调用 `dma_graph_request_mode()`；
3. 修改 `desired_mode` 和按键计数。

回调不会执行串口发送、延时或 HAL Q 队列操作。

### 8.4 为什么要修改两个链接字

DMA 可能已经取走了某一个旧的 CLLR 后继。如果只修改当前所在环的一个出口，DMA 可能短暂走到旧路径。因此代码对两个分支出口采用有顺序的更新：

- 切换到报警：先让 A2 指向 A1，形成完整报警环；再让 N6 指向 A1；
- 切回正常：先让 N6 指向 N1，形成完整正常环；再让 A2 指向 N1。

两个链接字都始终指向静态有效节点，不在运行期间重建队列。写入过程暂时关闭中断，并使用 `__DMB()`、`__DSB()` 保证写入顺序和可见性。

## 9. CPU、DMA 和中断的分工

### CPU 负责

- 系统启动和外设配置；
- 构造一次 DMA 节点图；
- 检查时钟、PWM 和地址窗口；
- 处理按键 EXTI；
- 捕获错误并停机；
- 正常运行时执行 `WFI`。

### LPDMA1_CH0 负责

- 按 TIM2 更新请求搬运 PWM 样本；
- 按 USART2 TX 请求搬运日志字节；
- 在节点结束时读取下一个链接；
- 按 N6/A2 的 CLLR 进入正常环或报警环。

### 中断负责

- EXTI13：接收按键并完成去抖/改链；
- LPDMA1_CH0：只处理 DMA 错误路径；
- 其他 UART/TIM/DMA 完成中断：最终应用启动图后主动关闭，避免完成中断参与动画推进。

这里的“CPU 不参与逐节点调度”并不是 CPU 永远不运行，而是 CPU 不需要每个样本、每条日志或每个节点完成时被唤醒。

## 10. 错误诊断机制

### 10.1 应用故障处理

`app_fault()` 做的事情很简单：

```text
ready = 0
保存 fault_detail
保存 fault
数据同步屏障
关闭可屏蔽中断
永久 WFI
```

这样发生故障后，调试器仍可以读取第一现场。工程没有依赖“故障时再发送一条串口日志”，因为 UART 或 DMA 本身可能就是故障源。

### 10.2 初始化错误分类

`App/app_diagnostics.c` 通过 GNU linker `--wrap` 包装以下函数：

- `mx_tim2_init()`；
- `mx_usart2_uart_init()`；
- `HAL_DMA_Init()`；
- `HAL_DMA_SetConfigPeriphDirectXfer()`；
- `HAL_DMA_IRQHandler()`。

前四类 wrapper 把第一个初始化错误保存到 `g_app_diagnostics.init_fault` 和 `init_detail`。最后一个 wrapper 在进入真实 HAL DMA IRQ handler 前调用 `dma_graph_capture_error()`，保存可能会被 HAL 清除的 DMA 寄存器现场。

### 10.3 主要诊断变量

| 变量 | 作用 |
| --- | --- |
| `g_app_diagnostics.ready` | 应用是否完成初始化并进入运行状态 |
| `g_app_diagnostics.fault` | 最终应用故障类别，0 表示无故障 |
| `g_app_diagnostics.fault_detail` | 关联的 HAL 状态、寄存器或地址信息 |
| `g_app_diagnostics.tim_kernel_hz` | 启动时读取的 TIM2 内核时钟 |
| `g_app_diagnostics.desired_mode` | 软件请求的正常/报警目标模式 |
| `g_app_diagnostics.button_events` | 通过消抖并被应用接受的按键次数 |
| `g_dma_graph.node_count` | 两个队列中的节点总数，正常应为 8 |
| `g_dma_graph.starts` | DMA 图启动次数，正常应为 1 |
| `g_dma_graph.relinks` | 成功提交的运行时改链次数 |
| `g_dma_graph.rejected_relinks` | 图未启动或模式非法时拒绝的次数 |
| `g_dma_graph.error_count` | DMA 错误回调次数 |
| `g_dma_graph.error_snapshot_valid` | DMA 错误寄存器快照是否有效 |
| `g_button_diagnostics.raw_edges` | 匹配触发沿的原始事件数 |
| `g_button_diagnostics.accepted_events` | 通过消抖的事件数 |
| `g_button_diagnostics.rejected_edges` | 消抖窗口内被拒绝的事件数 |
| `g_button_diagnostics.debounce_restarts` | TIM6 去抖窗口启动次数 |

### 10.4 当前故障类别

`App/app_main.h` 中的故障枚举主要分为：

- `APP_FAULT_SYSTEM_INIT`：CubeMX2 系统初始化失败；
- `APP_FAULT_PWM_CONFIGURATION`：实际 TIM2 时钟、PSC、ARR 与应用常量不一致；
- `APP_FAULT_PWM_START`：TIM2 PWM 启动失败；
- `APP_FAULT_UART_TX`：USART2 TX 就绪等待失败；
- `APP_FAULT_BUTTON_START`：按键 EXTI/TIM6 配置失败；
- `APP_FAULT_DMA_CONFIG`、`APP_FAULT_DMA_START`：DMA 图配置或启动失败；
- `APP_FAULT_DMA_RUNTIME`：DMA 报告 DTE、ULE 或 USE 等运行时错误；
- `APP_FAULT_NODE_MEMORY`：节点地址、对齐或 64 KB 链接窗口不合法；
- `APP_FAULT_NODE_BUILD`、`APP_FAULT_QUEUE_BUILD`：节点或 HAL Q 构建失败；
- `APP_FAULT_WAVEFORM`：PWM 表生成或边界检查失败；
- `APP_FAULT_ILLEGAL_RELINK`：运行时改链请求不合法；
- `APP_FAULT_UART_INIT`、`APP_FAULT_TIM_INIT`、`APP_FAULT_DMA_INIT`：wrapper 捕获的初始化失败。

## 11. 为什么要检查 DMA 节点地址

DMA linked-list 的 CLLR 链接字段不是一个普通的完整 C 指针。当前线性寻址方式只编码链接地址的一部分，因此所有节点必须：

- 位于 DMA 可访问的 SRAM；
- 按 4 字节对齐；
- 整个数组位于同一个 64 KB 链接窗口；
- 地址和节点格式符合 HAL/LL 对线性节点的要求。

`dma_graph.c` 使用静态、对齐的数组：

```c
_Alignas(4) static hal_dma_node_t nodes[NODE_COUNT];
```

启动前还检查数组的首地址、尾地址和链接窗口。如果把节点数组改成不合适的段、放入 DMA 不可见的内存，程序会在 `APP_FAULT_NODE_MEMORY` 停机，而不是让 DMA 运行到不可预测的地址。

## 12. 构建、烧录和观察

### 12.1 编译

在 `05_DMA_List/05_DMA_List_cmake` 目录执行：

```powershell
cube cmake --preset debug_GCC_NUCLEO-C542RC
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
```

输出文件通常位于：

```text
05_DMA_List/05_DMA_List_cmake/build/debug_GCC_NUCLEO-C542RC/
├── 05_DMA_List.elf
└── 05_DMA_List.map
```

`build/` 是可重新生成的目录，已经被 `.gitignore` 忽略，不应提交到 Git。

### 12.2 烧录

当前版本删除了仓库内的自动烧录脚本。可以使用 STM32CubeProgrammer 的 Cube CLI，示例：

```powershell
cube programmer -c port=SWD sn=<探头序列号> mode=UR reset=HWrst freq=1000 `
  -w .\build\debug_GCC_NUCLEO-C542RC\05_DMA_List.elf -v -rst
```

如果只有一块板，也可以根据本机 CubeProgrammer 版本省略 `sn`。烧录前确认 ELF 路径、探头序列号和目标开发板，避免把程序写入错误设备。

### 12.3 串口观察

通过 ST-LINK Virtual COM Port 打开实际 COM 端口，设置：

```text
波特率：115200
数据位：8
停止位：1
校验：无
流控：无
```

正常运行时可观察到：

```text
[NORMAL] Cycle Start
[NORMAL] LED Max
[NORMAL] Cycle Done
```

按下 B1 并完成一次链表收敛后，可观察到：

```text
[ALARM] Active
```

串口日志只能证明 USART2 TX 节点正在运行。LED 的实际 PWM 波形和按键切换延迟仍应使用示波器、逻辑分析仪或肉眼结合时序测试确认。

### 12.4 调试器观察

连接 GDB 后，优先查看：

```text
g_app_diagnostics
g_dma_graph
g_button_diagnostics
```

正常稳定运行的典型关系是：

```text
ready                  = 1
fault                  = APP_FAULT_NONE
g_dma_graph.node_count = 8
g_dma_graph.starts     = 1
g_dma_graph.error_count= 0
```

按键触发后，`accepted_events` 和 `button_events` 应增加；每个成功的模式提交会使 `g_dma_graph.relinks` 增加。`desired_mode` 代表软件请求，不保证 DMA 在写入瞬间已经完成切换。

## 13. 源码阅读路线

建议按以下顺序阅读：

### 第一步：读应用入口

打开 `05_DMA_List/05_DMA_List_cmake/main.c`，确认它只负责系统初始化和调用 `app_run()`。

然后读 `App/app_main.c`，重点看：

- 启动参数检查；
- `dma_graph_build()` 和 `dma_graph_start()` 的调用顺序；
- `mode_button_event()`；
- `WFI` 主循环。

### 第二步：读应用常量

打开 `Config/app_config.h`，先建立时间尺度：

```text
PWM 频率       = 1000 Hz
渐亮           = 1000 ms
渐暗           = 1000 ms
暗态           = 500 ms
报警半周期     = 75 ms
报警次数       = 4
按键消抖       = 40 ms
```

### 第三步：读波形和日志

先读 `pwm_waveform.c`，看三个表怎样生成；再读 `logger_dma.c`，看日志字符串和长度怎样提供给 DMA 节点。

### 第四步：读 DMA 图

重点阅读 `Drivers_App/dma_graph.c` 的四部分：

1. `build_node()`：一个节点如何描述 UART/TIM 传输；
2. `dma_graph_build()`：两个 HAL Q 如何组装；
3. `dma_graph_start()`：启动顺序和中断策略；
4. `dma_graph_request_mode()`：两个 CLLR 后继如何安全切换。

### 第五步：读底层生成代码

最后读：

- `generated/hal/mx_tim2.c`：TIM2、PA5、LPDMA1_CH0 的初始配置；
- `generated/hal/mx_usart2.c`：USART2、PA2 和 UART 相关 DMA 的初始配置；
- `generated/hal/mx_gpio_default.c`：PC13/EXTI13；
- `generated/hal/mx_system.c`：总初始化顺序。

不要一开始从 `stm32c5xx_drivers/` 目录读起。那里是供应商 HAL/LL 实现，代码量很大，不能直接说明本实验的业务逻辑。

## 14. 修改工程时的规则

### 14.1 修改硬件配置

引脚、时钟、TIM2、USART2、DMA request、NVIC 等硬件参数，优先修改：

```text
05_DMA_List/05_DMA_List.ioc2
```

然后重新用 CubeMX2 生成工程。重新生成后要检查：

- `main.c` 是否仍然调用 `app_run()`；
- `CMakeLists.txt` 的自定义源文件是否仍在 `target_sources()`；
- `--wrap` 链接参数是否仍存在；
- `mx_tim2.c` 的 PSC、ARR、DMA request 是否仍符合应用常量；
- `mx_usart2.c` 的 TX 引脚和波特率是否正确；
- PC13/EXTI13 的触发沿是否与 `APP_BUTTON_ACTIVE_HIGH` 一致。

### 14.2 修改应用效果

以下参数可以从 `Config/app_config.h` 修改：

| 参数 | 修改效果 |
| --- | --- |
| `APP_FADE_UP_MS` | 渐亮时长和渐亮表大小 |
| `APP_FADE_DOWN_MS` | 渐暗时长和渐暗表大小 |
| `APP_DARK_HOLD_MS` | N6 暗态保持时间 |
| `APP_ALARM_HALF_PERIOD_MS` | 报警亮/灭半周期 |
| `APP_ALARM_FLASH_COUNT` | 报警表中的亮灭组数 |
| `APP_BUTTON_DEBOUNCE_MS` | TIM6 去抖窗口 |
| `APP_PWM_FREQUENCY_HZ` | 应用对 TIM2 更新频率的预期 |

如果修改 `APP_PWM_FREQUENCY_HZ`、`APP_TIM_KERNEL_HZ`、`APP_PWM_PRESCALER` 或 `APP_PWM_PERIOD_COUNTS`，必须同步检查 CubeMX2 生成的 TIM2 配置。文件中的 `_Static_assert` 只能检查常量之间的数学关系，不能自动修改硬件寄存器。

### 14.3 不要做的事情

- 不要把 DMA 波形表改成 `const` 并期待它仍然能被当前 DMA 配置读取；
- 不要把 `nodes[]` 放到 DMA 不可访问的内存或跨越链接窗口；
- 不要运行期间调用 HAL Q 重建队列；
- 不要在按键 EXTI 回调中加入阻塞延时或串口发送；
- 不要在 DMA 图运行期间调用 `HAL_UART_Transmit_DMA()` 抢占 UART 传输资源；
- 不要直接修改 `generated/`、HAL 或 LL 源码来解决应用问题，除非明确知道重新生成后如何恢复；
- 不要只修改 `app_config.h` 而不重新构建和检查 ELF。

## 15. 当前验证边界

当前简化版本已经完成：

- CMake 配置成功；
- GCC/Ninja 编译成功；
- `05_DMA_List.elf` 链接成功；
- 删除旧阶段代码后，最终 DMA/PWM/UART/按键路径仍被纳入构建；
- 本地和远程 Git 分支同步。

当前这次简化之后还没有在本轮操作中重新完成：

- 开发板烧录；
- PA5 PWM 示波器波形确认；
- USART2 实物日志确认；
- B1 实物按键切换和 40 ms 去抖确认；
- 极端 DMA 取链竞争压力测试。

因此，`e8ad3a0` 可以作为可编译的简化版本，但第一次使用时仍应按“编译 -> 烧录 -> 串口观察 -> LED/按键确认 -> GDB 读取诊断变量”的顺序做一次实物验收。

## 16. 一句话总结

这个工程的核心不是“CPU 写一个 LED 数值”，而是“CPU 只在启动时搭好一张 DMA 执行图，之后由 TIM2 和 USART2 的硬件请求驱动 LPDMA1_CH0 自动运行；按键只修改图上的两个分支链接”。
