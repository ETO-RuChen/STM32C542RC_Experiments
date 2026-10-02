/**
 * @file app_diagnostics.c
 * @brief 用 GNU ld --wrap 截获初始化失败，并在 HAL 清通道前保存 DMA 现场。
 *
 * 这样无需直接修改 CubeMX2 生成的 mx_tim2.c、mx_usart2.c 或 HAL IRQ 源码。
 * 更换 HAL 版本、链接器或函数名后，必须同步复核 CMake 中的 --wrap 入口。
 */
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
  /* 保留第一个初始化错误，避免后续失败覆盖真正的根因。 */
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
  /* HAL 错误路径会先复位通道再回调，必须在进入真实 IRQ handler 前抓寄存器。 */
  dma_graph_capture_error(hdma);
#endif
  __real_HAL_DMA_IRQHandler(hdma);
}
