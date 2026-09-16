# APP_PHASE=7 already flashed. This script injects EXTI events and halts in ISR.
# ./Tools/read_state.ps1 -Commands 'source Tools/test_relink.gdb'
# Recheck disassembly after toolchain/optimization changes: +12 must be pop {r7,pc}.
# Debugger pauses distort timing; this is not a worst-case latency test.
set pagination off
set confirm off
if APP_PHASE != 7 || g_app_diagnostics.ready != 1 || g_dma_graph.starts != 1
  echo ERROR: test requires the matching APP_PHASE=7 ELF and a ready graph.\n
  quit 1
end
disassemble EXTI13_IRQHandler
if *(unsigned short *)(EXTI13_IRQHandler+12) != 0xbd80
  echo ERROR: unexpected handler epilogue; inspect disassembly before testing.\n
  quit 1
end
hbreak *EXTI13_IRQHandler+12
set $raw = g_button_diagnostics.raw_edges
set $accepted = g_button_diagnostics.accepted_events
set $rejected = g_button_diagnostics.rejected_edges
set $relinks = g_dma_graph.relinks
set $i = 0
set $failed = 0
while $i < 32 && $failed == 0
  set EXTI->SWIER1 = 0x2000
  continue
  set $target = (unsigned int)&nodes[0]
  if g_app_diagnostics.desired_mode == 1
    set $target = (unsigned int)&nodes[6]
  end
  set $target = $target & 0xfffc
  set $failed = g_app_diagnostics.fault != 0 || g_dma_graph.error_count != 0 || (nodes[5].regs[5] & 0xfffc) != $target || (nodes[7].regs[5] & 0xfffc) != $target
  set $i = $i + 1
end
set $accepted = g_button_diagnostics.accepted_events - $accepted
set $rejected = g_button_diagnostics.rejected_edges - $rejected
set $failed = $failed || g_button_diagnostics.raw_edges - $raw != 32 || $accepted + $rejected != 32 || g_dma_graph.relinks - $relinks != $accepted || g_dma_graph.starts != 1 || g_dma_graph.completions != 0
printf "RELINK_RESULT failed=%u events=%u accepted=%u rejected=%u starts=%u errors=%u\n", $failed, $i, $accepted, $rejected, g_dma_graph.starts, g_dma_graph.error_count
delete breakpoints
