#include "bsp_vcp.h"
#include "app_config.h"
#include "mx_usart2.h"

hal_status_t bsp_vcp_write(const char *text, uint32_t size_byte)
{
  if ((text == NULL) || (size_byte == 0U))
  {
    return HAL_INVALID_PARAM;
  }
  return HAL_UART_Transmit(mx_usart2_uart_gethandle(), text, size_byte,
                           APP_UART_TIMEOUT_MS);
}
