#ifndef APP_HAL_OVERRIDES_H
#define APP_HAL_OVERRIDES_H

/* Force-included for ALL application, generated and HAL translation units.
   Load the generated configuration first, then apply project-owned settings
   before any HAL types are declared. This avoids editing generated files. */
#include "stm32c5xx_hal_conf.h"
#undef USE_HAL_DMA_GET_LAST_ERRORS
#define USE_HAL_DMA_GET_LAST_ERRORS 1U
#undef USE_HAL_UART_GET_LAST_ERRORS
#define USE_HAL_UART_GET_LAST_ERRORS 1U
#undef USE_HAL_CHECK_PARAM
#define USE_HAL_CHECK_PARAM 1U

#endif
