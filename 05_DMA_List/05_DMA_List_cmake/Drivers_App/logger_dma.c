#include "logger_dma.h"
#include <stddef.h>

static const char cycle_start[] = "[NORMAL] Cycle Start\r\n";
static const char led_max[] = "[NORMAL] LED Max\r\n";
static const char cycle_done[] = "[NORMAL] Cycle Done\r\n";
static const char alarm[] = "[ALARM] Active\r\n";
static const logger_dma_buffer_t buffers[LOG_COUNT] =
{
  {cycle_start, sizeof(cycle_start) - 1U},
  {led_max, sizeof(led_max) - 1U},
  {cycle_done, sizeof(cycle_done) - 1U},
  {alarm, sizeof(alarm) - 1U}
};

logger_dma_buffer_t logger_dma_get(logger_dma_id_t id)
{
  if ((uint32_t)id >= LOG_COUNT) { return (logger_dma_buffer_t){NULL, 0U}; }
  return buffers[id];
}
