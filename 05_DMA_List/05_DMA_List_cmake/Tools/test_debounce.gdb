# Freeze only TIM6 during debug halts, excluding host pause time from debounce.
# TIM2 and DMA continue. The first event must be accepted, following 31 rejected.
# Run with a ready APP_PHASE=7 board, after >40 ms of normal operation.
# stm32c542xx.h: APB1LFZR offset 0x008, DBG_TIM6_STOP bit 4.
set $freeze_reg = (unsigned int *)(DBGMCU_BASE + 8)
set $freeze_before = *$freeze_reg
set *$freeze_reg = $freeze_before | 0x10
source Tools/test_relink.gdb
set *$freeze_reg = $freeze_before
set $failed = $failed || $accepted != 1 || $rejected != 31
printf "DEBOUNCE_RESULT failed=%u accepted=%u rejected=%u\n", $failed, $accepted, $rejected
