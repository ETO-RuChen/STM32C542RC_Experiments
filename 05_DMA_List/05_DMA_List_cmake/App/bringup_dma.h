#ifndef BRINGUP_DMA_H
#define BRINGUP_DMA_H

#include <stdint.h>

typedef struct
{
  uint32_t timer_completions;
  uint32_t uart_completions;
  uint32_t start_ms;
  uint32_t elapsed_ms;
  uint32_t remaining_bytes;
  uint32_t final_ccr;
  uint32_t dma_errors;
  uint32_t uart_errors;
  uint32_t passed;
} bringup_dma_diagnostics_t;

extern volatile bringup_dma_diagnostics_t g_bringup_dma;
_Noreturn void bringup_direct_run(void);

#endif
