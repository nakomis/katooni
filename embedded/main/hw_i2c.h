/* The one I²C bus: expanders, FUSB302B, INA226s. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t hw_i2c_init(void);
esp_err_t hw_i2c_add(uint8_t addr, i2c_master_dev_handle_t *dev);
esp_err_t hw_i2c_probe(uint8_t addr);
esp_err_t hw_i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, const uint8_t *data, size_t len);
esp_err_t hw_i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len);
