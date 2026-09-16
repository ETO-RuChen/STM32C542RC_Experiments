#ifndef PWM_WAVEFORM_H
#define PWM_WAVEFORM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  const uint32_t *samples;
  uint32_t count;
} pwm_waveform_t;

bool pwm_waveform_init(void);
pwm_waveform_t pwm_waveform_up(void);
pwm_waveform_t pwm_waveform_down(void);
pwm_waveform_t pwm_waveform_alarm(void);

#endif
