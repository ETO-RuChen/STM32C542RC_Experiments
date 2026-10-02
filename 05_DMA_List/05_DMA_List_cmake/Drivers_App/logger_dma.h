#ifndef LOGGER_DMA_H
#define LOGGER_DMA_H

#include <stdint.h>

/** Flash 中的不可变日志编号；LOG_COUNT 只作为边界，不是有效日志。 */
typedef enum
{
  LOG_CYCLE_START,
  LOG_LED_MAX,
  LOG_CYCLE_DONE,
  LOG_ALARM,
  LOG_COUNT
} logger_dma_id_t;

/** 供 UART DMA 节点使用的内存地址和精确字节数（不发送结尾 NUL）。 */
typedef struct
{
  const char *text;
  uint32_t size_byte;
} logger_dma_buffer_t;

/** 根据编号返回日志视图；编号越界时返回 {NULL, 0}。 */
logger_dma_buffer_t logger_dma_get(logger_dma_id_t id);

#endif
