#include "dma_graph.h"
#include "app_main.h"
#include "app_config.h"
#include "bsp_led.h"
#include "pwm_waveform.h"
#include "mx_tim2.h"
#include "stm32c5xx_ll_tim.h"

volatile dma_graph_diagnostics_t g_dma_graph;

#if APP_PHASE >= 3
/* Reserve one contiguous SRAM array for the future normal and alarm nodes. */
enum { N1, N2, N3, N4, N5, N6, A1, A2, NODE_COUNT };
_Alignas(4) static hal_dma_node_t nodes[NODE_COUNT];
static hal_q_t normal_q;
static hal_dma_handle_t *graph_dma;

static void graph_error(hal_dma_handle_t *hdma)
{
  DMA_Channel_TypeDef *channel = HAL_DMA_GetLLInstance(hdma);
  ++g_dma_graph.error_count;
  g_dma_graph.error_codes = HAL_DMA_GetLastErrorCodes(hdma);
  g_dma_graph.error_cllr = channel->CLLR;
  g_dma_graph.error_src = channel->CSAR;
  g_dma_graph.error_dest = channel->CDAR;
  app_fault(APP_FAULT_DMA_RUNTIME, g_dma_graph.error_codes);
}

static void graph_complete(hal_dma_handle_t *hdma)
{
  (void)hdma;
  LL_TIM_DisableDMAReq_UPDATE(TIM2);
  g_dma_graph.elapsed_ms = HAL_GetTick() - g_dma_graph.start_ms;
  ++g_dma_graph.completions;
}

static void timer_node(uint32_t index, pwm_waveform_t waveform)
{
  hal_dma_node_config_t config = {0};
  config.xfer.request = HAL_LPDMA1_REQUEST_TIM2_UPD;
  config.xfer.direction = HAL_DMA_DIRECTION_MEMORY_TO_PERIPH;
  config.xfer.src_inc = HAL_DMA_SRC_ADDR_INCREMENTED;
  config.xfer.dest_inc = HAL_DMA_DEST_ADDR_FIXED;
  config.xfer.src_data_width = HAL_DMA_SRC_DATA_WIDTH_WORD;
  config.xfer.dest_data_width = HAL_DMA_DEST_DATA_WIDTH_WORD;
  config.xfer.priority = HAL_DMA_PRIORITY_HIGH;
  config.hw_request_mode = HAL_DMA_HARDWARE_REQUEST_BURST;
  config.flow_ctrl_mode = HAL_DMA_FLOW_CONTROL_DMA;
  config.xfer_event_mode = HAL_DMA_LINKEDLIST_XFER_EVENT_Q;
  config.trigger.source = HAL_LPDMA1_TRIGGER_EXTI0;
  config.trigger.polarity = HAL_DMA_TRIGGER_POLARITY_MASKED;
  config.trigger.mode = HAL_DMA_TRIGGER_SINGLE_BURST_TRANSFER;
  config.data_handling.trunc_padd = HAL_DMA_DEST_DATA_TRUNC_LEFT_PADD_ZERO;
  config.src_addr = (uint32_t)waveform.samples;
  config.dest_addr = (uint32_t)&TIM2->CCR1;
  config.size_byte = waveform.count * sizeof(uint32_t);
  app_check_status(HAL_DMA_FillNodeConfig(&nodes[index], &config,
                    HAL_DMA_NODE_LINEAR_ADDRESSING), APP_FAULT_NODE_BUILD);
}

void dma_graph_build(void)
{
  const uint32_t first = (uint32_t)&nodes[0];
  const uint32_t last = first + sizeof(nodes) - 1U;
  /* Current linker RAM is exactly 0x20000000..0x2000ffff (SRAM1+SRAM2). */
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
  app_check_status(HAL_DMA_RegisterXferCpltCallback(graph_dma, graph_complete), APP_FAULT_DMA_CONFIG);
  timer_node(N2, pwm_waveform_up());
  timer_node(N4, pwm_waveform_down());
  app_check_status(HAL_Q_Init(&normal_q, &HAL_DMA_LinearAddressing_DescOps), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_InsertNode_Tail(&normal_q, &nodes[N2]), APP_FAULT_QUEUE_BUILD);
  app_check_status(HAL_Q_InsertNode_Tail(&normal_q, &nodes[N4]), APP_FAULT_QUEUE_BUILD);
  g_dma_graph.node_count = normal_q.node_nbr;
}

void dma_graph_start(void)
{
  __DMB();
  g_dma_graph.start_ms = HAL_GetTick();
  app_check_status(HAL_DMA_StartLinkedListXfer_IT_Opt(graph_dma, &normal_q, HAL_DMA_OPT_IT_NONE),
                    APP_FAULT_DMA_START);
  ++g_dma_graph.starts;
  LL_TIM_EnableDMAReq_UPDATE(TIM2);
  app_check_status(bsp_led_start(), APP_FAULT_PWM_START);
}
#endif
