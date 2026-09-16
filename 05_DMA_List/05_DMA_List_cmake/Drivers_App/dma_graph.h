#ifndef DMA_GRAPH_H
#define DMA_GRAPH_H

#include <stdint.h>
#include <stdbool.h>
#include "app_mode.h"

typedef struct
{
  uint32_t node_count;
  uint32_t first_address;
  uint32_t last_address;
  uint32_t starts;
  uint32_t completions;
  uint32_t start_ms;
  uint32_t elapsed_ms;
  uint32_t error_count;
  uint32_t error_codes;
  uint32_t error_cllr;
  uint32_t error_src;
  uint32_t error_dest;
  uint32_t relinks;
  uint32_t rejected_relinks;
} dma_graph_diagnostics_t;

extern volatile dma_graph_diagnostics_t g_dma_graph;
void dma_graph_build(void);
void dma_graph_start(void);
bool dma_graph_request_mode(app_mode_t mode);

#endif
