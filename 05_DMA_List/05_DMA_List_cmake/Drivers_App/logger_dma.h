#ifndef LOGGER_DMA_H
#define LOGGER_DMA_H

#include <stdint.h>

typedef enum { LOG_CYCLE_START, LOG_LED_MAX, LOG_CYCLE_DONE, LOG_ALARM, LOG_COUNT } logger_dma_id_t;
typedef struct
{
  const char *text;
  uint32_t size_byte;
} logger_dma_buffer_t;

logger_dma_buffer_t logger_dma_get(logger_dma_id_t id);

#endif
