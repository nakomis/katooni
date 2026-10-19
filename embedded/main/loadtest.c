#include <stdbool.h>

#include "loadtest.h"

#include "board_pins.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hw_i2c.h"

#define REG_CONFIG 0x00
#define REG_SHUNT 0x01   /* 2.5 µV per bit, signed */
#define REG_BUS 0x02     /* 1.25 mV per bit */

/* 16 averages, 1.1 ms conversions, shunt and bus continuous: about 35 ms per result. */
#define CONFIG_AVG16 0x4527

static const char *TAG = "load";
static i2c_master_dev_handle_t s_cur, s_far;

static esp_err_t wr16(i2c_master_dev_handle_t d, uint8_t reg, uint16_t v)
{
    uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)v};   /* INA226 registers are big-endian */
    return hw_i2c_write_reg(d, reg, b, 2);
}

static esp_err_t rd16(i2c_master_dev_handle_t d, uint8_t reg, int16_t *v)
{
    uint8_t b[2];
    esp_err_t err = hw_i2c_read_reg(d, reg, b, 2);
    *v = (int16_t)((b[0] << 8) | b[1]);
    return err;
}

static void out(gpio_num_t pin)
{
    gpio_set_level(pin, 0);
    gpio_config_t c = {.pin_bit_mask = 1ULL << pin, .mode = GPIO_MODE_OUTPUT};
    gpio_config(&c);
    gpio_set_level(pin, 0);
}

esp_err_t loadtest_init(void)
{
    out(KAT_PIN_LOAD_EN);
    out(KAT_PIN_FAR_SHORT);
    out(KAT_PIN_A_GND_EN);
    ESP_RETURN_ON_ERROR(hw_i2c_add(KAT_INA_CURRENT_ADDR, &s_cur), TAG, "add");
    ESP_RETURN_ON_ERROR(hw_i2c_add(KAT_INA_FAR_ADDR, &s_far), TAG, "add");
    ESP_RETURN_ON_ERROR(wr16(s_cur, REG_CONFIG, CONFIG_AVG16), TAG, "INA226 0x%02x", KAT_INA_CURRENT_ADDR);
    ESP_RETURN_ON_ERROR(wr16(s_far, REG_CONFIG, CONFIG_AVG16), TAG, "INA226 0x%02x", KAT_INA_FAR_ADDR);
    return ESP_OK;
}

void loadtest_ground_a_end(bool on)
{
    gpio_set_level(KAT_PIN_A_GND_EN, on);
}

esp_err_t loadtest_measure(int32_t *drop_uv, int32_t *current_ua)
{
    *drop_uv = *current_ua = 0;
    loadtest_ground_a_end(true);
    gpio_set_level(KAT_PIN_FAR_SHORT, 1);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(KAT_PIN_LOAD_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(80));     /* two full averaged conversions */
    int16_t shunt = 0, bus = 0, far = 0;
    esp_err_t err = rd16(s_cur, REG_SHUNT, &shunt);
    if (!err) err = rd16(s_cur, REG_BUS, &bus);
    if (!err) err = rd16(s_far, REG_SHUNT, &far);
    gpio_set_level(KAT_PIN_LOAD_EN, 0);
    gpio_set_level(KAT_PIN_FAR_SHORT, 0);
    loadtest_ground_a_end(false);
    ESP_RETURN_ON_ERROR(err, TAG, "read");
    int32_t shunt_uv = (int32_t)shunt * 5 / 2;
    *current_ua = shunt_uv * 1000 / KAT_SHUNT_MILLIOHM;
    *drop_uv = (int32_t)bus * 1250 - (int32_t)far * 5 / 2;
    return ESP_OK;
}
