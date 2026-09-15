/**
  ******************************************************************************
  * file           : app_uart_demo.c
  * brief          : UART printf, DMA receive and DMA echo demo.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "app_uart_demo.h"
#include "mx_basic_stdio_app.h"

#include <stdio.h>
#include <string.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define APP_UART_RX_BUFFER_SIZE          (64U)
#define APP_UART_TX_BUFFER_SIZE          (96U)
#define APP_UART_TX_DONE_TIMEOUT_MS      (1000U)

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static hal_uart_handle_t *g_pUartHandle;

static uint8_t g_UartRxBuffer[APP_UART_RX_BUFFER_SIZE];
static uint8_t g_UartTxBuffer[APP_UART_TX_BUFFER_SIZE];

static volatile uint32_t g_UartRxLength;
static volatile hal_uart_rx_event_types_t g_UartRxEvent;
static volatile uint8_t g_UartRxFrameReady;
static volatile uint8_t g_UartTxDmaBusy;
static volatile uint8_t g_UartErrorDetected;

/* Private functions prototype -----------------------------------------------*/
static hal_status_t App_UartStartDmaReceive(void);
static void App_UartProcessReceivedFrame(void);
static void App_UartProcessError(void);
static void App_UartPrintHex(const uint8_t *pData, uint32_t size);
static void App_UartPrintAscii(const uint8_t *pData, uint32_t size);
static hal_status_t App_UartEchoByDma(const uint8_t *pData, uint32_t size);
static hal_status_t App_UartWaitTxDmaDone(uint32_t timeoutMs);
static const char *App_UartRxEventToString(hal_uart_rx_event_types_t event);

/* Exported functions --------------------------------------------------------*/

/**
  * brief:  初始化串口实验需要的软件状态。
  * retval: system_status_t
  */
system_status_t App_UartDemo_Init(void)
{
  g_pUartHandle = mx_usart2_uart_gethandle();
  if (g_pUartHandle == NULL)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  /* mx_basic_stdio 将 printf 的输出绑定到 CubeMX2 选择的 USART2。 */
  if (mx_basic_stdio_init() != SYSTEM_OK)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  g_UartRxLength = 0U;
  g_UartRxEvent = HAL_UART_RX_EVENT_TC;
  g_UartRxFrameReady = 0U;
  g_UartTxDmaBusy = 0U;
  g_UartErrorDetected = 0U;

  printf("\r\n\r\n");
  printf("========================================\r\n");
  printf(" STM32C542 UART communication demo\r\n");
  printf(" USART2: PA2(TX), PA3(RX), 115200-8-N-1\r\n");
  printf(" printf: enabled by mx_basic_stdio\r\n");
  printf(" RX: DMA receive-to-idle callback\r\n");
  printf(" TX: DMA echo after frame received\r\n");
  printf("========================================\r\n");
  printf("Send text from serial terminal, then press Enter.\r\n\r\n");

  if (App_UartStartDmaReceive() != HAL_OK)
  {
    printf("[UART] Failed to start DMA reception.\r\n");
    return SYSTEM_PERIPHERAL_ERROR;
  }

  return SYSTEM_OK;
}

/**
  * brief:  串口实验主任务，处理接收帧和错误恢复。
  * retval: None
  */
void App_UartDemo_Process(void)
{
  App_UartProcessReceivedFrame();
  App_UartProcessError();
}

/* Private functions ---------------------------------------------------------*/

/**
  * brief:  启动一次 DMA 接收，收到空闲帧或缓冲区满后进入接收完成回调。
  * retval: hal_status_t
  */
static hal_status_t App_UartStartDmaReceive(void)
{
  g_UartRxLength = 0U;
  g_UartRxFrameReady = 0U;
  memset(g_UartRxBuffer, 0, sizeof(g_UartRxBuffer));

  return HAL_UART_ReceiveToIdle_DMA_Opt(g_pUartHandle,
                                        g_UartRxBuffer,
                                        APP_UART_RX_BUFFER_SIZE,
                                        HAL_UART_OPT_DMA_RX_IT_NONE);
}

/**
  * brief:  在主循环中处理一帧已接收的数据，避免在中断里 printf。
  * retval: None
  */
static void App_UartProcessReceivedFrame(void)
{
  uint32_t rxLength;
  hal_uart_rx_event_types_t rxEvent;

  if (g_UartRxFrameReady == 0U)
  {
    return;
  }

  rxLength = g_UartRxLength;
  rxEvent = g_UartRxEvent;
  g_UartRxFrameReady = 0U;

  if (rxLength > APP_UART_RX_BUFFER_SIZE)
  {
    rxLength = APP_UART_RX_BUFFER_SIZE;
  }

  (void)App_UartWaitTxDmaDone(APP_UART_TX_DONE_TIMEOUT_MS);

  printf("[UART] RX event: %s, length: %lu byte(s)\r\n",
         App_UartRxEventToString(rxEvent),
         (unsigned long)rxLength);
  printf("[UART] RX ASCII: ");
  App_UartPrintAscii(g_UartRxBuffer, rxLength);
  printf("\r\n");
  printf("[UART] RX HEX  : ");
  App_UartPrintHex(g_UartRxBuffer, rxLength);
  printf("\r\n");

  if (App_UartEchoByDma(g_UartRxBuffer, rxLength) != HAL_OK)
  {
    printf("[UART] DMA echo failed.\r\n");
  }

  if (App_UartStartDmaReceive() != HAL_OK)
  {
    printf("[UART] Failed to restart DMA reception.\r\n");
  }
}

/**
  * brief:  处理 UART 错误标志，并重新启动 DMA 接收。
  * retval: None
  */
static void App_UartProcessError(void)
{
  if (g_UartErrorDetected == 0U)
  {
    return;
  }

  g_UartErrorDetected = 0U;
  (void)App_UartWaitTxDmaDone(APP_UART_TX_DONE_TIMEOUT_MS);
  printf("[UART] Error detected, restart DMA reception.\r\n");
  (void)App_UartStartDmaReceive();
}

/**
  * brief:  以十六进制格式打印接收数据，方便观察不可见字符。
  * retval: None
  */
static void App_UartPrintHex(const uint8_t *pData, uint32_t size)
{
  uint32_t index;

  for (index = 0U; index < size; index++)
  {
    printf("%02X ", pData[index]);
  }
}

/**
  * brief:  以可读字符打印接收数据，不可打印字符用点号替代。
  * retval: None
  */
static void App_UartPrintAscii(const uint8_t *pData, uint32_t size)
{
  uint32_t index;

  for (index = 0U; index < size; index++)
  {
    uint8_t data = pData[index];

    if ((data >= 0x20U) && (data <= 0x7EU))
    {
      printf("%c", data);
    }
    else
    {
      printf(".");
    }
  }
}

/**
  * brief:  使用 DMA 回显刚收到的数据，演示串口发送功能。
  * retval: hal_status_t
  */
static hal_status_t App_UartEchoByDma(const uint8_t *pData, uint32_t size)
{
  static const uint8_t kEchoPrefix[] = "Echo by DMA: ";
  static const uint8_t kEchoSuffix[] = "\r\n";
  uint32_t txLength = 0U;

  if (size > (APP_UART_TX_BUFFER_SIZE - sizeof(kEchoPrefix) - sizeof(kEchoSuffix)))
  {
    size = APP_UART_TX_BUFFER_SIZE - sizeof(kEchoPrefix) - sizeof(kEchoSuffix);
  }

  memcpy(&g_UartTxBuffer[txLength], kEchoPrefix, sizeof(kEchoPrefix) - 1U);
  txLength += sizeof(kEchoPrefix) - 1U;
  memcpy(&g_UartTxBuffer[txLength], pData, size);
  txLength += size;
  memcpy(&g_UartTxBuffer[txLength], kEchoSuffix, sizeof(kEchoSuffix) - 1U);
  txLength += sizeof(kEchoSuffix) - 1U;

  g_UartTxDmaBusy = 1U;
  if (HAL_UART_Transmit_DMA(g_pUartHandle, g_UartTxBuffer, txLength) != HAL_OK)
  {
    g_UartTxDmaBusy = 0U;
    return HAL_ERROR;
  }

  return HAL_OK;
}

/**
  * brief:  等待 DMA 发送完成，防止 printf 与 DMA 同时抢 USART2 发送通道。
  * retval: hal_status_t
  */
static hal_status_t App_UartWaitTxDmaDone(uint32_t timeoutMs)
{
  uint32_t elapsedMs = 0U;

  while (g_UartTxDmaBusy != 0U)
  {
    if (elapsedMs >= timeoutMs)
    {
      return HAL_TIMEOUT;
    }

    HAL_Delay(1U);
    elapsedMs++;
  }

  return HAL_OK;
}

/**
  * brief:  将 HAL 的接收事件转换为便于串口打印的文本。
  * retval: const char *
  */
static const char *App_UartRxEventToString(hal_uart_rx_event_types_t event)
{
  switch (event)
  {
    case HAL_UART_RX_EVENT_IDLE:
      return "IDLE";

    case HAL_UART_RX_EVENT_TC:
      return "TRANSFER_COMPLETE";

    case HAL_UART_RX_EVENT_RTO:
      return "TIMEOUT";

    case HAL_UART_RX_EVENT_CHAR_MATCH:
      return "CHAR_MATCH";

    default:
      return "UNKNOWN";
  }
}

/**
  * brief:  DMA 发送完成回调，只更新状态，不在中断上下文中打印。
  * retval: None
  */
void HAL_UART_TxCpltCallback(hal_uart_handle_t *huart)
{
  if (huart == g_pUartHandle)
  {
    g_UartTxDmaBusy = 0U;
  }
}

/**
  * brief:  DMA 接收完成回调，保存长度和事件类型，交给主循环处理。
  * retval: None
  */
void HAL_UART_RxCpltCallback(hal_uart_handle_t *huart,
                             uint32_t size_byte,
                             hal_uart_rx_event_types_t rx_event)
{
  if (huart == g_pUartHandle)
  {
    g_UartRxLength = size_byte;
    g_UartRxEvent = rx_event;
    g_UartRxFrameReady = 1U;
  }
}

/**
  * brief:  UART 错误回调，主循环会重新启动接收。
  * retval: None
  */
void HAL_UART_ErrorCallback(hal_uart_handle_t *huart)
{
  if (huart == g_pUartHandle)
  {
    g_UartTxDmaBusy = 0U;
    g_UartErrorDetected = 1U;
  }
}
