#include "hw_i2c.h"

#include "board_pins.h"
#include "esp_check.h"

static const char *TAG = "i2c";
static i2c_master_bus_handle_t s_bus;

esp_err_t hw_i2c_init(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = KAT_PIN_I2C_SDA,
        .scl_io_num = KAT_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,   /* the board has 4.7 kΩ pull-ups */
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&cfg, &s_bus), TAG, "bus");
    return ESP_OK;
}

esp_err_t hw_i2c_add(uint8_t addr, i2c_master_dev_handle_t *dev)
{
    i2c_device_config_t d = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = KAT_I2C_HZ,
    };
    return i2c_master_bus_add_device(s_bus, &d, dev);
}

esp_err_t hw_i2c_probe(uint8_t addr)
{
    return i2c_master_probe(s_bus, addr, 20);
}

esp_err_t hw_i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, const uint8_t *data, size_t len)
{
    uint8_t buf[1 + 64];
    if (len > sizeof buf - 1) return ESP_ERR_INVALID_SIZE;
    buf[0] = reg;
    for (size_t i = 0; i < len; i++) buf[1 + i] = data[i];
    return i2c_master_transmit(dev, buf, len + 1, 50);
}

esp_err_t hw_i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, 50);
}
