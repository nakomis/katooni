#include "buzzer.h"

#include "board_pins.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MODE LEDC_LOW_SPEED_MODE
#define TIMER LEDC_TIMER_0
#define CHANNEL LEDC_CHANNEL_0

static const char *TAG = "buzzer";

esp_err_t buzzer_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = TIMER,
        .freq_hz = 2700,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&t), TAG, "timer");
    ledc_channel_config_t c = {
        .gpio_num = KAT_PIN_BUZZER,
        .speed_mode = MODE,
        .channel = CHANNEL,
        .timer_sel = TIMER,
        .duty = 0,
    };
    return ledc_channel_config(&c);
}

static void tone(uint32_t hz, uint32_t ms)
{
    ledc_set_freq(MODE, TIMER, hz);
    ledc_set_duty(MODE, CHANNEL, 512);
    ledc_update_duty(MODE, CHANNEL);
    vTaskDelay(pdMS_TO_TICKS(ms));
    ledc_set_duty(MODE, CHANNEL, 0);
    ledc_update_duty(MODE, CHANNEL);
}

void buzzer_play(beep_t b)
{
    switch (b) {
    case BEEP_SAME_BAG:
        tone(2700, 60);
        break;
    case BEEP_NEW_BAG:
        tone(2200, 70);
        vTaskDelay(pdMS_TO_TICKS(40));
        tone(3300, 90);
        break;
    case BEEP_FAULTY:
        tone(600, 400);
        break;
    }
}
