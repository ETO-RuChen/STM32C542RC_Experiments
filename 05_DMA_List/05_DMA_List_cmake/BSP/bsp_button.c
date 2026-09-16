#include "bsp_button.h"
#include "app_config.h"
#include "mx_gpio_default.h"
#include "stm32c5xx_ll_tim.h"

volatile bsp_button_diagnostics_t g_button_diagnostics;
static bsp_button_callback_t button_callback;

static void button_trigger(hal_exti_handle_t *hexti, hal_exti_trigger_t trigger)
{
  (void)hexti;
#if APP_BUTTON_ACTIVE_HIGH
  const hal_exti_trigger_t selected_edge = HAL_EXTI_TRIGGER_RISING;
#else
  const hal_exti_trigger_t selected_edge = HAL_EXTI_TRIGGER_FALLING;
#endif
  if (((uint32_t)trigger & (uint32_t)selected_edge) == 0U)
  {
    return;
  }

#if APP_PHASE >= 7
  /* TIM6 stops itself after 40 ms without an interrupt. Reading CEN avoids a
     wraparound-prone timestamp and keeps working when SysTick is suspended. */
  uint32_t rejected = LL_TIM_IsEnabledCounter(TIM6);
  LL_TIM_DisableCounter(TIM6);
  LL_TIM_SetCounter(TIM6, 0U);
  LL_TIM_ClearFlag_UPDATE(TIM6);
  LL_TIM_EnableCounter(TIM6);
  ++g_button_diagnostics.debounce_restarts;
#else
  const uint32_t now = HAL_GetTick();
  const uint32_t elapsed = now - g_button_diagnostics.last_edge_ms;
  g_button_diagnostics.last_edge_ms = now;
  uint32_t rejected = (elapsed < APP_BUTTON_DEBOUNCE_MS);
#endif
  ++g_button_diagnostics.raw_edges;

  /* Unsigned subtraction also handles the HAL millisecond tick wrapping. */
  if (rejected != 0U)
  {
    ++g_button_diagnostics.rejected_edges;
    return;
  }

  ++g_button_diagnostics.accepted_events;
  button_callback();
}

hal_status_t bsp_button_start(bsp_button_callback_t callback)
{
  if (callback == NULL)
  {
    return HAL_INVALID_PARAM;
  }

#if APP_PHASE >= 7
  const uint32_t clock_hz = HAL_RCC_TIM_GetKernelClkFreq(TIM6);
  const uint32_t divider = clock_hz / APP_DEBOUNCE_TIMER_HZ;
  if ((clock_hz % APP_DEBOUNCE_TIMER_HZ != 0U) || (divider == 0U) || (divider > 65536U))
  {
    return HAL_ERROR;
  }
  HAL_RCC_TIM6_EnableClock();
  HAL_RCC_TIM6_Reset();
  HAL_CORTEX_NVIC_DisableIRQ(TIM6_IRQn);
  LL_TIM_SetPrescaler(TIM6, divider - 1U);
  LL_TIM_SetAutoReload(TIM6, APP_BUTTON_DEBOUNCE_MS * APP_DEBOUNCE_TIMER_HZ / 1000U - 1U);
  LL_TIM_EnableOnePulseMode(TIM6);
  LL_TIM_GenerateEvent_UPDATE(TIM6);
  LL_TIM_ClearFlag_UPDATE(TIM6);
#endif

  hal_exti_handle_t *hexti = mx_gpio_default_exti13_gethandle();
  hal_exti_config_t config = {0};
  HAL_EXTI_GetConfig(hexti, &config);
#if APP_BUTTON_ACTIVE_HIGH
  config.trigger = HAL_EXTI_TRIGGER_RISING;
#else
  config.trigger = HAL_EXTI_TRIGGER_FALLING;
#endif
  hal_status_t status = HAL_EXTI_SetConfig(hexti, &config);
  if (status != HAL_OK)
  {
    return status;
  }
  status = HAL_EXTI_RegisterTriggerCallback(hexti, button_trigger);
  if (status != HAL_OK)
  {
    return status;
  }

  button_callback = callback;
#if APP_PHASE < 7
  g_button_diagnostics.last_edge_ms = HAL_GetTick() - APP_BUTTON_DEBOUNCE_MS;
#endif
  HAL_EXTI_ClearPending(hexti, HAL_EXTI_TRIGGER_RISING_FALLING);
  /* NVIC is already configured by CubeMX2; the EXTI line still needs enabling. */
  return HAL_EXTI_Enable(hexti, HAL_EXTI_MODE_INTERRUPT);
}
