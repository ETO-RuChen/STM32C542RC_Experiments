#include "bringup_dma.h"
#include "app_main.h"
#include "bsp_led.h"
#include "mx_tim2.h"
#include "mx_usart2.h"
#include "stm32c5xx_ll_tim.h"

volatile bringup_dma_diagnostics_t g_bringup_dma;

#if APP_PHASE == 2
/* Filled once at startup. No CPU writes after the transfer starts. */
static uint32_t direct_lut[2000];
static const char start_log[] = "[P2] USART2 DMA TX OK; TIM2_UP DMA: 2000 samples / 2000ms\r\n";
static const char done_log[] = "[P2] PASS: TIM2 DMA complete, CCR1=0, UART DMA complete\r\n";

static void timer_done(hal_dma_handle_t *hdma)
{
  LL_TIM_DisableDMAReq_UPDATE(TIM2);
  g_bringup_dma.elapsed_ms = HAL_GetTick() - g_bringup_dma.start_ms;
  g_bringup_dma.remaining_bytes = HAL_DMA_GetDirectXferRemainingDataByte(hdma);
  g_bringup_dma.final_ccr = TIM2->CCR1;
  ++g_bringup_dma.timer_completions;
}

static void timer_error(hal_dma_handle_t *hdma)
{
  g_bringup_dma.dma_errors = HAL_DMA_GetLastErrorCodes(hdma);
  app_fault(APP_FAULT_DMA_RUNTIME, g_bringup_dma.dma_errors);
}

void HAL_UART_TxCpltCallback(hal_uart_handle_t *huart)
{
  (void)huart;
  ++g_bringup_dma.uart_completions;
}

void HAL_UART_ErrorCallback(hal_uart_handle_t *huart)
{
  g_bringup_dma.uart_errors = HAL_UART_GetLastErrorCodes(huart);
  app_fault(APP_FAULT_UART_RUNTIME, g_bringup_dma.uart_errors);
}

static void wait_completions(uint32_t uart_count, uint32_t timer_count, uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  while ((g_bringup_dma.uart_completions < uart_count)
         || (g_bringup_dma.timer_completions < timer_count))
  {
    if (HAL_GetTick() - start > timeout_ms)
    {
      app_fault(APP_FAULT_DMA_TIMEOUT, timeout_ms);
    }
    __WFI();
  }
}

_Noreturn void bringup_direct_run(void)
{
  for (uint32_t i = 0U; i < 1000U; ++i)
  {
    direct_lut[i] = i;
    direct_lut[i + 1000U] = 999U - i;
  }

  hal_dma_handle_t *hdma = mx_tim2_gethandle()->hdma[HAL_TIM_DMA_ID_UPD];
  app_check_status(HAL_DMA_RegisterXferCpltCallback(hdma, timer_done), APP_FAULT_DMA_CONFIG);
  app_check_status(HAL_DMA_RegisterXferErrorCallback(hdma, timer_error), APP_FAULT_DMA_CONFIG);

  app_check_status(HAL_UART_Transmit_DMA_Opt(mx_usart2_uart_gethandle(), start_log,
                    sizeof(start_log) - 1U, HAL_UART_OPT_DMA_TX_IT_NONE), APP_FAULT_UART_TX);
  wait_completions(1U, 0U, 100U);

  __DMB();
  app_check_status(HAL_DMA_StartDirectXfer_IT_Opt(hdma, (uint32_t)direct_lut,
                    (uint32_t)&TIM2->CCR1, sizeof(direct_lut), HAL_DMA_OPT_IT_NONE),
                    APP_FAULT_DMA_START);
  g_bringup_dma.start_ms = HAL_GetTick();
  LL_TIM_EnableDMAReq_UPDATE(TIM2);
  app_check_status(bsp_led_start(), APP_FAULT_PWM_START);
  g_app_diagnostics.ready = 1U;
  wait_completions(1U, 1U, 2500U);

  if ((g_bringup_dma.remaining_bytes != 0U) || (g_bringup_dma.final_ccr != 0U)
      || (g_bringup_dma.elapsed_ms < 1998U) || (g_bringup_dma.elapsed_ms > 2002U))
  {
    app_fault(APP_FAULT_DMA_VERIFY, g_bringup_dma.elapsed_ms);
  }
  app_check_status(HAL_UART_Transmit_DMA_Opt(mx_usart2_uart_gethandle(), done_log,
                    sizeof(done_log) - 1U, HAL_UART_OPT_DMA_TX_IT_NONE), APP_FAULT_UART_TX);
  wait_completions(2U, 1U, 100U);
  g_bringup_dma.passed = 1U;
  for (;;)
  {
    __WFI();
  }
}
#endif
