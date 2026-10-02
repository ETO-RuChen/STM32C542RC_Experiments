# 实验 02：UART、printf 与 DMA 收发

[返回仓库总览](../README.md)

本实验使用板载 ST-LINK 虚拟串口演示 USART2 通信：启动时通过 `printf` 输出说明，接收端使用 LPDMA Receive-to-Idle，收到一帧后在主循环打印 ASCII/HEX，并用 DMA 回显原始数据。

## 1. 实验目标

- 理解 USART2 与 ST-LINK Virtual COM Port 的关系。
- 理解 HAL2 句柄 getter，而不是依赖全局 `huart2`。
- 使用 DMA 接收和发送。
- 理解“中断回调只记录事件，主循环处理数据”的结构。
- 认识 `basic_stdio` 如何把 `printf` 接到 UART。

## 2. 硬件配置

| 资源 | 配置 | 用途 |
| --- | --- | --- |
| USART2_TX | PA2，AF7 | MCU 向电脑发送 |
| USART2_RX | PA3，AF7 | 电脑向 MCU 发送 |
| 串口格式 | 115200，8 data bits，no parity，1 stop bit | ST-LINK VCP |
| LPDMA1_CH0 | USART2_TX，memory-to-peripheral | DMA 回显 |
| LPDMA1_CH1 | USART2_RX，peripheral-to-memory | DMA 接收 |
| USART2/LPDMA IRQ | NVIC priority 0 | 事件和完成回调 |
| 接收缓冲区 | 64 bytes | 单帧演示缓冲 |
| 发送缓冲区 | 96 bytes | `Echo by DMA` 帧 |

## 3. 程序流程

```text
main()
  -> mx_system_init()
       -> 配置 USART2、LPDMA1_CH0/CH1、IRQ 和 basic_stdio 接线
  -> App_UartDemo_Init()
       -> mx_usart2_uart_gethandle()
       -> mx_basic_stdio_init()
       -> printf 启动横幅
       -> HAL_UART_ReceiveToIdle_DMA_Opt(..., 64 bytes, ...)
  -> while (1)
       -> App_UartProcessReceivedFrame()
       -> App_UartProcessError()
```

接收事件路径：

```text
USART2 空闲事件或 DMA 接收完成
  -> USART2_IRQHandler() / LPDMA1_CH1_IRQHandler()
  -> HAL2 UART/DMA IRQ handler
  -> HAL_UART_RxCpltCallback(handle, size, event)
       -> 只保存长度、事件类型和 frame-ready 标志
  -> 主循环看到标志
       -> 打印 ASCII
       -> 打印 HEX
       -> HAL_UART_Transmit_DMA() 回显
       -> 重新启动 Receive-to-Idle
```

发送完成后，`HAL_UART_TxCpltCallback()` 清除 `g_UartTxDmaBusy`。错误回调只设置错误标志，主循环负责打印并尝试重启接收。

## 4. 为什么回调里不 printf

UART/DMA 回调在中断上下文运行。若在回调中直接 `printf`：

- 格式化和阻塞发送可能占用较长时间。
- 可能与正在运行的 DMA TX 竞争 USART2。
- 其他同级或更低优先级中断会被延迟。
- 错误恢复路径更难分析。

因此本实验用 `volatile` 变量把事件从中断传给主循环。`volatile` 只保证编译器真正读写这些变量，不等于通用的线程同步机制；本例状态简单，且接收会在主循环处理后才重新启动。

## 5. basic_stdio 与 DMA 的分工

- `mx_basic_stdio_init()` 把普通 `printf` 输出绑定到 USART2。
- 启动横幅、ASCII/HEX 分析行使用 basic stdio。
- `Echo by DMA: ...` 使用 `HAL_UART_Transmit_DMA()`。
- 开始新的前台打印前，`App_UartWaitTxDmaDone()` 会等待上一次 DMA TX 完成，避免两条发送路径同时使用 USART2。

这是一份教学演示，不是高吞吐串口协议。持续无间隔地发送大量数据时，在主循环打印和重启 RX 的窗口内可能丢字节。正式协议通常需要循环 DMA 缓冲、队列或双缓冲，并定义明确的帧边界。

## 6. 关键文件

| 文件 | 重点 |
| --- | --- |
| `02_UART.ioc2` | USART2、LPDMA、NVIC 和 basic stdio 配置源 |
| `02_UART_cmake/main.c` | 系统初始化和应用轮询入口 |
| `02_UART_cmake/app_uart_demo.c` | 接收状态、格式化、DMA 回显和回调 |
| `02_UART_cmake/app_uart_demo.h` | 应用公开接口 |
| `02_UART_cmake/generated/hal/mx_usart2.c` | UART 参数、GPIO、两个 DMA channel、IRQ |
| `02_UART_cmake/generated/utilities/` | basic stdio 的生成接线 |
| `02_UART_cmake/utilities/basic_stdio/` | 标准输出实现 |
| `02_UART_cmake/CMakeLists.txt` | 将 `app_uart_demo.c` 加入固件 |

HAL2 的 USART2 句柄在 `mx_usart2.c` 中是 `static hal_uart_handle_t hUSART2`。应用通过 `mx_usart2_uart_gethandle()` 获取指针，这与很多 HAL1 教程中的全局 `UART_HandleTypeDef huart2` 不同。

## 7. 构建与串口设置

```powershell
cd .\02_UART\02_UART_cmake
cube cmake --preset debug_GCC_NUCLEO-C542RC
cube cmake --build --preset debug_GCC_NUCLEO-C542RC
```

串口终端：

```text
Baud rate : 115200
Data bits : 8
Parity    : None
Stop bits : 1
Flow ctrl : None
Line end  : CRLF（推荐）
```

产物位于 `02_UART_cmake/build/debug_GCC_NUCLEO-C542RC/02_UART.elf`。完整烧录命令见 [构建烧录与操作手册](../Docs/02_构建烧录与操作手册.md)。

## 8. 预期输出

复位后先看到启动横幅。发送 `Hello STM32` 并附带 CRLF，输出类似：

```text
[UART] RX event: IDLE, length: 13 byte(s)
[UART] RX ASCII: Hello STM32..
[UART] RX HEX  : 48 65 6C 6C 6F 20 53 54 4D 33 32 0D 0A
Echo by DMA: Hello STM32
```

CR 和 LF 不可打印，所以 ASCII 行显示为两个点，HEX 行保留 `0D 0A`。

## 9. 验收与排错

1. 烧录前打开 115200 8-N-1 串口。
2. 复位后应出现完整启动横幅。
3. 发送短文本和 CRLF，应同时看到事件、长度、ASCII、HEX 和回显。
4. 发送不可打印字节时，确认 ASCII 显示点号而 HEX 保留真实值。
5. 发送超过 64 字节时，可能拆成多帧，这是固定接收缓冲的预期行为。

没有输出时依次检查：是否选择了 ST-LINK VCP 的正确 COM 口、端口是否被其他程序占用、参数是否为 115200 8-N-1、PA2/PA3 是否仍由 USART2 配置，以及 `App_UartDemo_Init()` 是否返回错误。

## 10. 可以继续练习

- 给接收数据增加行结束符解析，而不是只依赖 IDLE。
- 用环形缓冲区承接连续数据。
- 给每帧增加长度、命令和校验字段。
- 把前台 `printf` 也改为排队式 DMA TX，避免阻塞。
- 用调试器观察 `g_UartRxLength`、`g_UartRxEvent` 和三个状态标志。

目录与 HAL2 API 对照见 [HAL2 文件结构文档](../Docs/01_工程全景与阅读路线.md)。
