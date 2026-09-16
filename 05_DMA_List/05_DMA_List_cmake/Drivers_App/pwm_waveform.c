#include "pwm_waveform.h"
#include "app_config.h"

#define UP_COUNT (APP_FADE_UP_MS * APP_PWM_FREQUENCY_HZ / 1000U)
#define DOWN_COUNT (APP_FADE_DOWN_MS * APP_PWM_FREQUENCY_HZ / 1000U)

static uint32_t up[UP_COUNT];
static uint32_t down[DOWN_COUNT];

_Static_assert(UP_COUNT > 1U && DOWN_COUNT > 1U, "Fade needs at least two samples");
_Static_assert(sizeof(up) <= 65535U && sizeof(down) <= 65535U, "DMA block size overflow");

bool pwm_waveform_init(void)
{
  for (uint32_t i = 0; i < UP_COUNT; ++i)
  {
    up[i] = i * (APP_PWM_PERIOD_COUNTS - 1U) / (UP_COUNT - 1U);
    if (up[i] >= APP_PWM_PERIOD_COUNTS) { return false; }
  }
  for (uint32_t i = 0; i < DOWN_COUNT; ++i)
  {
    down[i] = (APP_PWM_PERIOD_COUNTS - 1U)
              - i * (APP_PWM_PERIOD_COUNTS - 1U) / (DOWN_COUNT - 1U);
    if (down[i] >= APP_PWM_PERIOD_COUNTS) { return false; }
  }
  return true;
}

pwm_waveform_t pwm_waveform_up(void)
{
  return (pwm_waveform_t){up, UP_COUNT};
}

pwm_waveform_t pwm_waveform_down(void)
{
  return (pwm_waveform_t){down, DOWN_COUNT};
}
