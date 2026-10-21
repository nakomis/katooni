/* A passive buzzer on LEDC PWM. */
#pragma once

#include "esp_err.h"

typedef enum {
    BEEP_SAME_BAG,   /* one short beep */
    BEEP_NEW_BAG,    /* two rising beeps */
    BEEP_FAULTY,     /* one low buzz */
} beep_t;

esp_err_t buzzer_init(void);
void buzzer_play(beep_t b);   /* blocks for the length of the pattern (under half a second) */
