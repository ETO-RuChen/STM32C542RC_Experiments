#ifndef BSP_VCP_H
#define BSP_VCP_H

#include "stm32_hal.h"

/* Blocking foreground transmission for P1 bring-up only. */
hal_status_t bsp_vcp_write(const char *text, uint32_t size_byte);

#endif
