#include "bsp_button.h"
#include "app_config.h"
#include "mx_gpio_default.h"

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

  const uint32_t now = HAL_GetTick();
  const uint32_t elapsed = now - g_button_diagnostics.last_edge_ms;
  g_button_diagnostics.last_edge_ms = now;
  ++g_button_diagnostics.raw_edges;

  /* Unsigned subtraction also handles the HAL millisecond tick wrapping. */
  if (elapsed < APP_BUTTON_DEBOUNCE_MS)
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
  g_button_diagnostics.last_edge_ms = HAL_GetTick() - APP_BUTTON_DEBOUNCE_MS;
  HAL_EXTI_ClearPending(hexti, HAL_EXTI_TRIGGER_RISING_FALLING);
  /* NVIC is already configured by CubeMX2; the EXTI line still needs enabling. */
  return HAL_EXTI_Enable(hexti, HAL_EXTI_MODE_INTERRUPT);
}
