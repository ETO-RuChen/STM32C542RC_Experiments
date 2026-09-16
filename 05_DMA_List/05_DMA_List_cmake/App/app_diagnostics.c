/* GNU ld wrappers keep generated initialization/IRQ sources unchanged.
   Recheck these entry points when regenerating with a new HAL version. */
#include "app_main.h"
#include "app_config.h"
#include "dma_graph.h"
#include "mx_tim2.h"
#include "mx_usart2.h"

hal_tim_handle_t *__real_mx_tim2_init(void);
hal_uart_handle_t *__real_mx_usart2_uart_init(void);
hal_status_t __real_HAL_DMA_Init(hal_dma_handle_t *hdma, hal_dma_channel_t instance);
hal_status_t __real_HAL_DMA_SetConfigPeriphDirectXfer(hal_dma_handle_t *hdma,
                                                    const hal_dma_direct_xfer_config_t *config);
void __real_HAL_DMA_IRQHandler(hal_dma_handle_t *hdma);

static void init_failed(app_fault_t fault, uint32_t detail)
{
  if (g_app_diagnostics.init_fault == APP_FAULT_NONE)
  {
    g_app_diagnostics.init_fault = fault;
    g_app_diagnostics.init_detail = detail;
  }
}

hal_tim_handle_t *__wrap_mx_tim2_init(void)
{
  hal_tim_handle_t *handle = __real_mx_tim2_init();
  if (handle == NULL) { init_failed(APP_FAULT_TIM_INIT, HAL_ERROR); }
  return handle;
}

hal_uart_handle_t *__wrap_mx_usart2_uart_init(void)
{
  hal_uart_handle_t *handle = __real_mx_usart2_uart_init();
  if (handle == NULL) { init_failed(APP_FAULT_UART_INIT, HAL_ERROR); }
  return handle;
}

hal_status_t __wrap_HAL_DMA_Init(hal_dma_handle_t *hdma, hal_dma_channel_t instance)
{
  hal_status_t status = __real_HAL_DMA_Init(hdma, instance);
  if (status != HAL_OK) { init_failed(APP_FAULT_DMA_INIT, (uint32_t)status); }
  return status;
}

hal_status_t __wrap_HAL_DMA_SetConfigPeriphDirectXfer(hal_dma_handle_t *hdma,
                                                    const hal_dma_direct_xfer_config_t *config)
{
  hal_status_t status = __real_HAL_DMA_SetConfigPeriphDirectXfer(hdma, config);
  if (status != HAL_OK) { init_failed(APP_FAULT_DMA_INIT, (uint32_t)status); }
  return status;
}

void __wrap_HAL_DMA_IRQHandler(hal_dma_handle_t *hdma)
{
#if APP_PHASE >= 3
  /* HAL's error path resets the channel before calling graph_error. */
  dma_graph_capture_error(hdma);
#endif
  __real_HAL_DMA_IRQHandler(hdma);
}
