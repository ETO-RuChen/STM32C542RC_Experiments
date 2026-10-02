/**
 * @file dma_graph.c
 * @brief 在一个 LPDMA1_CH0 上构建 UART/TIM 混合节点图并支持运行时改链。
 *
 * 节点只在启动前通过 HAL_Q 构建。运行后只在短临界区内更新 N6/A2
 * 描述符的完整 CLLR word；DMA 不停止、不重建。
 */
#include "dma_graph.h"
#include "app_main.h"
#include "app_config.h"
#include "bsp_led.h"
#include "pwm_waveform.h"
#include "logger_dma.h"
#include "mx_tim2.h"
#include "stm32c5xx_ll_tim.h"
#include "stm32c5xx_ll_usart.h"

volatile dma_graph_diagnostics_t g_dma_graph;

/* All normal/alarm nodes share one SRAM link-address window. */
enum { N1, N2, N3, N4, N5, N6, A1, A2, NODE_COUNT };
_Alignas(4) static hal_dma_node_t nodes[NODE_COUNT];
static hal_q_t normal_q;
static hal_q_t alarm_q;
static hal_dma_handle_t *graph_dma;
/* Fixed source: each update reads this same SRAM word during dark hold. */
static uint32_t dark_zero;
static uint32_t branch_words[2];
static bool graph_started;

_Static_assert(LL_DMA_NODE_CLLR_REG_OFFSET == 5U, "Recheck static node format");

void dma_graph_capture_error(hal_dma_handle_t *hdma)
{
  DMA_Channel_TypeDef *channel = HAL_DMA_GetLLInstance(hdma);
  if ((hdma != graph_dma) || (g_dma_graph.error_snapshot_valid != 0U)
      || ((channel->CSR & (DMA_CSR_DTEF | DMA_CSR_ULEF | DMA_CSR_USEF)) == 0U))
  {
    return;
  }
  g_dma_graph.error_csr = channel->CSR;
  g_dma_graph.error_ccr = channel->CCR;
  g_dma_graph.error_cbr1 = channel->CBR1;
  g_dma_graph.error_ctr1 = channel->CTR1;
  g_dma_graph.error_ctr2 = channel->CTR2;
  g_dma_graph.error_cllr = channel->CLLR;
  g_dma_graph.error_src = channel->CSAR;
  g_dma_graph.error_dest = channel->CDAR;
  g_dma_graph.error_snapshot_valid = 1U;
}

static void graph_error(hal_dma_handle_t *hdma)
{
  ++g_dma_graph.error_count;
  g_dma_graph.error_codes = HAL_DMA_GetLastErrorCodes(hdma);
  app_fault(APP_FAULT_DMA_RUNTIME, g_dma_graph.error_codes);
}

static void build_node(uint32_t index, uint32_t source, uint32_t destination,
                       uint32_t size, bool timer, bool fixed_source)
{
  /* 每个节点带完整传输配置，因此同一通道能在节点边界切换 UART/TIM request。 */
  hal_dma_node_config_t config = {0};
  config.xfer.request = timer ? HAL_LPDMA1_REQUEST_TIM2_UPD : HAL_LPDMA1_REQUEST_USART2_TX;
  config.xfer.direction = HAL_DMA_DIRECTION_MEMORY_TO_PERIPH;
  config.xfer.src_inc = fixed_source ? HAL_DMA_SRC_ADDR_FIXED : HAL_DMA_SRC_ADDR_INCREMENTED;
  config.xfer.dest_inc = HAL_DMA_DEST_ADDR_FIXED;
  config.xfer.src_data_width = timer ? HAL_DMA_SRC_DATA_WIDTH_WORD : HAL_DMA_SRC_DATA_WIDTH_BYTE;
  config.xfer.dest_data_width = timer ? HAL_DMA_DEST_DATA_WIDTH_WORD : HAL_DMA_DEST_DATA_WIDTH_BYTE;
  config.xfer.priority = HAL_DMA_PRIORITY_HIGH;
  config.hw_request_mode = HAL_DMA_HARDWARE_REQUEST_BURST;
  config.flow_ctrl_mode = HAL_DMA_FLOW_CONTROL_DMA;
  config.xfer_event_mode = HAL_DMA_LINKEDLIST_XFER_EVENT_Q;
  config.trigger.source = HAL_LPDMA1_TRIGGER_EXTI0;
  config.trigger.polarity = HAL_DMA_TRIGGER_POLARITY_MASKED;
  config.trigger.mode = HAL_DMA_TRIGGER_SINGLE_BURST_TRANSFER;
  config.data_handling.trunc_padd = HAL_DMA_DEST_DATA_TRUNC_LEFT_PADD_ZERO;
  config.src_addr = source;
  config.dest_addr = destination;
  config.size_byte = size;
  if ((index >= NODE_COUNT) || (size == 0U) || (size > 65535U)
      || (timer && (((source | destination | size) & 3U) != 0U)))
  {
    app_fault(APP_FAULT_NODE_BUILD, index);
  }
  app_check_status(HAL_DMA_FillNodeConfig(&nodes[index], &config,
                    HAL_DMA_NODE_LINEAR_ADDRESSING), APP_FAULT_NODE_BUILD);
}

static void timer_node(uint32_t index, pwm_waveform_t waveform)
{
  build_node(index, (uint32_t)waveform.samples, (uint32_t)&TIM2->CCR1,
             waveform.count * sizeof(uint32_t), true, false);
}

static void uart_node(uint32_t index, logger_dma_id_t log)
{
  logger_dma_buffer_t buffer = logger_dma_get(log);
  build_node(index, (uint32_t)buffer.text, (uint32_t)&USART2->TDR,
             buffer.size_byte, false, false);
}

void dma_graph_build(void)
{
  /* 线性寻址节点的 link 只编码低 16 位，整个数组必须落在同一 64 KB 窗口。 */
  const uint32_t first = (uint32_t)&nodes[0];
  const uint32_t last = first + sizeof(nodes) - 1U;
  if (((first & 3U) != 0U) || (first < SRAM1_BASE) || (last >= 0x20010000U)
      || ((first & DMA_CLBAR_LBA) != (last & DMA_CLBAR_LBA)))
  {
    app_fault(APP_FAULT_NODE_MEMORY, first);
  }
  g_dma_graph.first_address = first;
  g_dma_graph.last_address = last;
  if (!pwm_waveform_init()) { app_fault(APP_FAULT_WAVEFORM, 0U); }

  graph_dma = mx_tim2_gethandle()->hdma[HAL_TIM_DMA_ID_UPD];
  hal_dma_linkedlist_xfer_config_t config =
  {
    .priority = HAL_DMA_PRIORITY_HIGH,
    .xfer_event_mode = HAL_DMA_LINKEDLIST_XFER_EVENT_Q
  };
  app_check_status(HAL_DMA_SetConfigLinkedListXfer(graph_dma, &config), APP_FAULT_DMA_CONFIG);
  app_check_status(HAL_DMA_SetLinkedListXferExecutionMode(graph_dma, HAL_DMA_LINKEDLIST_EXECUTION_Q),
                    APP_FAULT_DMA_CONFIG);
  app_check_status(HAL_DMA_RegisterXferErrorCallback(graph_dma, graph_error), APP_FAULT_DMA_CONFIG);

  timer_node(N2, pwm_waveform_up());
  timer_node(N4, pwm_waveform_down());
  uart_node(N1, LOG_CYCLE_START);
  uart_node(N3, LOG_LED_MAX);
  uart_node(N5, LOG_CYCLE_DONE);
  build_node(N6, (uint32_t)&dark_zero, (uint32_t)&TIM2->CCR1,
             APP_DARK_HOLD_MS * APP_PWM_FREQUENCY_HZ / 1000U * sizeof(uint32_t), true, true);

  app_check_status(HAL_Q_Init(&normal_q, &HAL_DMA_LinearAddressing_DescOps), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_InsertNode_Tail(&normal_q, &nodes[N1]), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_InsertNode_Tail(&normal_q, &nodes[N2]), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_InsertNode_Tail(&normal_q, &nodes[N3]), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_InsertNode_Tail(&normal_q, &nodes[N4]), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_InsertNode_Tail(&normal_q, &nodes[N5]), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_InsertNode_Tail(&normal_q, &nodes[N6]), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_SetCircularLinkQ_Head(&normal_q), APP_FAULT_QUEUE_BUILD);
  g_dma_graph.node_count = normal_q.node_nbr;

  uart_node(A1, LOG_ALARM);
  timer_node(A2, pwm_waveform_alarm());
  app_check_status(HAL_Q_Init(&alarm_q, &HAL_DMA_LinearAddressing_DescOps), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_InsertNode_Tail(&alarm_q, &nodes[A1]), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_InsertNode_Tail(&alarm_q, &nodes[A2]), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_SetCircularLinkQ_Head(&alarm_q), APP_FAULT_QUEUE_BUILD);
  g_dma_graph.node_count += alarm_q.node_nbr;

  /* 两个目标节点共享 CLBAR 窗口，运行时只写完整的后继 link word。 */
  branch_words[APP_MODE_NORMAL] = LL_DMA_UPDATE_ALL | ((uint32_t)&nodes[N1] & DMA_CLLR_LA);
  branch_words[APP_MODE_ALARM] = LL_DMA_UPDATE_ALL | ((uint32_t)&nodes[A1] & DMA_CLLR_LA);
  nodes[A2].regs[LL_DMA_NODE_CLLR_REG_OFFSET] = branch_words[APP_MODE_NORMAL];
}

void dma_graph_start(void)
{
  /* The graph owns DMA traffic: do not call HAL_UART_Transmit_DMA here. */
  LL_USART_Enable(USART2);
  uint32_t wait_start = HAL_GetTick();
  while (LL_USART_IsActiveFlag_TEACK(USART2) == 0U)
  {
    if (HAL_GetTick() - wait_start > APP_UART_TIMEOUT_MS)
    {
      app_fault(APP_FAULT_UART_TX, USART2->ISR);
    }
  }
  LL_USART_EnableDMAReq_TX(USART2);

  __DMB();
  app_check_status(HAL_DMA_StartLinkedListXfer(graph_dma, &normal_q), APP_FAULT_DMA_START);
  LL_DMA_EnableIT_DTE(LPDMA1_CH0);
  LL_DMA_EnableIT_ULE(LPDMA1_CH0);
  LL_DMA_EnableIT_USE(LPDMA1_CH0);
  HAL_CORTEX_NVIC_DisableIRQ(LPDMA2_CH0_IRQn);
  HAL_CORTEX_NVIC_DisableIRQ(USART2_IRQn);
  HAL_CORTEX_NVIC_DisableIRQ(TIM2_IRQn);
  ++g_dma_graph.starts;
  LL_TIM_EnableDMAReq_UPDATE(TIM2);
  app_check_status(bsp_led_start(), APP_FAULT_PWM_START);
  graph_started = true;
}

bool dma_graph_request_mode(app_mode_t mode)
{
  if (!graph_started || ((mode != APP_MODE_NORMAL) && (mode != APP_MODE_ALARM)))
  {
    ++g_dma_graph.rejected_relinks;
    return false;
  }

  /* 先闭合目标环，再打开另一环出口，保证 DMA 最终收敛到目标环。 */
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  volatile uint32_t *normal_link = &nodes[N6].regs[LL_DMA_NODE_CLLR_REG_OFFSET];
  volatile uint32_t *alarm_link = &nodes[A2].regs[LL_DMA_NODE_CLLR_REG_OFFSET];
  uint32_t word = branch_words[mode];
  if (mode == APP_MODE_ALARM)
  {
    *alarm_link = word;
    __DMB();
    *normal_link = word;
  }
  else
  {
    *normal_link = word;
    __DMB();
    *alarm_link = word;
  }
  __DSB();
  ++g_dma_graph.relinks;
  __set_PRIMASK(primask);
  return true;
}
