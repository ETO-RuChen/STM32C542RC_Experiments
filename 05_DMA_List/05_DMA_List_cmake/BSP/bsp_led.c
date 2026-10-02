#include "bsp_led.h"
#include "app_config.h"
#include "mx_tim2.h"

hal_status_t bsp_led_start(void)
{
  hal_tim_handle_t *htim = mx_tim2_gethandle();
  hal_status_t status = HAL_TIM_OC_StartChannel(htim, HAL_TIM_CHANNEL_1);
  if (status != HAL_OK)
  {
    return status;
  }

  /* HAL2 starts the output channel and counter with separate APIs. */
  return HAL_TIM_Start(htim);
}
