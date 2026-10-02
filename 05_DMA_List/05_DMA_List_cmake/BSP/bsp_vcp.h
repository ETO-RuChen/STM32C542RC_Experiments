#ifndef BSP_VCP_H
#define BSP_VCP_H

#include "stm32_hal.h"

/** P1 bring-up 使用：通过 USART2/ST-LINK VCP 前台阻塞发送。 */
hal_status_t bsp_vcp_write(const char *text, uint32_t size_byte);

#endif
