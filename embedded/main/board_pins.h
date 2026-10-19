/* Katooni's pin map: PLACEHOLDER VALUES.
 *
 * TODO(KATOO-1): the board generator (hardware/board/build.py) writes the real pins.h. Replace every
 * value marked TODO from it once the board is routed, keeping these macro names (or rename here and
 * in the drivers together). Nothing below has been checked against a schematic.
 *
 * What the firmware assumes about the circuit:
 * - One I²C bus: five MCP23017 expanders (16 lines each, 80 in all for 71 conductor lines), the
 *   FUSB302B (fixed at 0x22, so no expander may use 0x22) and two INA226s.
 * - Every connector pin is its own expander line, through a series resistor; none is tied to board
 *   ground. The expanders' internal ~100 kΩ pull-ups hold every line high; a scan drives one line
 *   low and reads which others follow.
 * - KAT_PIN_A_GND_EN switches the A-end USB-C's GND to board ground for the PD and load tests;
 *   KAT_PIN_LOAD_EN switches the test current into the A-end VBUS; KAT_PIN_FAR_SHORT shorts the
 *   B-end USB-C's VBUS to its GND to close the loop. All three are active high and off at reset.
 * - The ST7789 (240×320) is on SPI2 with its backlight switched by KAT_PIN_LCD_BL (GPIO_NUM_NC
 *   for a module whose backlight is always on). Inversion and offsets are in Kconfig.
 * - CONFIG_KATOONI_BOARD_BREADBOARD picks the breadboard profile below instead. */
#pragma once

#include "driver/gpio.h"
#include "sdkconfig.h"

#if CONFIG_KATOONI_BOARD_BREADBOARD
/* Breadboard prototype: an ESP32-S3-DevKitC with an Estardyn GMT020-02 (2.0" ST7789V, 240×320, SPI:
 * CS, DC, RST, SDA = MOSI, SCL = SCK, VCC, GND; no MISO, backlight always on). The I²C parts are
 * whatever is wired up: missing ones are reported at boot and skipped. The pins avoid the S3's
 * strapping pins (0, 3, 45, 46), native USB (19, 20) and the DevKitC's RGB LED (48). */
#define KAT_PIN_I2C_SDA GPIO_NUM_1
#define KAT_PIN_I2C_SCL GPIO_NUM_2
#define KAT_I2C_HZ 400000
#define KAT_EXPANDER_COUNT 5
#define KAT_EXPANDER_ADDRS {0x20, 0x21, 0x23, 0x24, 0x25}
#define KAT_LINE_MAP_ENTRY(n) {(uint8_t)((n) / 16), (uint8_t)((n) % 16)}
#define KAT_FUSB302_ADDR 0x22
#define KAT_PIN_FUSB302_INT GPIO_NUM_4
#define KAT_INA_CURRENT_ADDR 0x40
#define KAT_INA_FAR_ADDR 0x41
#define KAT_SHUNT_MILLIOHM 50
#define KAT_PIN_A_GND_EN GPIO_NUM_5
#define KAT_PIN_LOAD_EN GPIO_NUM_6
#define KAT_PIN_FAR_SHORT GPIO_NUM_7
#define KAT_LCD_HOST SPI2_HOST
#define KAT_PIN_LCD_MOSI GPIO_NUM_11          /* module SDA */
#define KAT_PIN_LCD_SCLK GPIO_NUM_12          /* module SCL */
#define KAT_PIN_LCD_CS GPIO_NUM_10
#define KAT_PIN_LCD_DC GPIO_NUM_9
#define KAT_PIN_LCD_RST GPIO_NUM_8
#define KAT_PIN_LCD_BL GPIO_NUM_NC            /* none: the backlight is always on */
#define KAT_LCD_HZ (40 * 1000 * 1000)
#define KAT_PIN_BUZZER GPIO_NUM_16

#else /* the Katooni PCB */

/* I²C */
#define KAT_PIN_I2C_SDA GPIO_NUM_38           /* TODO */
#define KAT_PIN_I2C_SCL GPIO_NUM_21           /* TODO */
#define KAT_I2C_HZ 400000

/* Expanders, in line order: expander n holds lines 16n..16n+15 unless KAT_LINE_MAP says otherwise. */
#define KAT_EXPANDER_COUNT 5
#define KAT_EXPANDER_ADDRS {0x20, 0x21, 0x23, 0x24, 0x25}   /* TODO; never 0x22 (the FUSB302B) */

/* Which expander pin each conductor line (katooni/lines.h numbering) is on: {expander, bit}, bit
 * 0-7 = port A, 8-15 = port B. TODO: from the routed board; this placeholder is simply in order. */
#define KAT_LINE_MAP_ENTRY(n) {(uint8_t)((n) / 16), (uint8_t)((n) % 16)}

/* USB PD PHY */
#define KAT_FUSB302_ADDR 0x22
#define KAT_PIN_FUSB302_INT GPIO_NUM_4        /* TODO */

/* Loop resistance: INA226 #1 measures the test current (shunt) and the A-end VBUS (bus voltage);
 * INA226 #2 senses at the B end. TODO: confirm the arrangement against the board's circuit. */
#define KAT_INA_CURRENT_ADDR 0x40
#define KAT_INA_FAR_ADDR 0x41
#define KAT_SHUNT_MILLIOHM 50                 /* TODO */
#define KAT_PIN_A_GND_EN GPIO_NUM_5           /* TODO */
#define KAT_PIN_LOAD_EN GPIO_NUM_6            /* TODO */
#define KAT_PIN_FAR_SHORT GPIO_NUM_7          /* TODO */

/* Display (ST7789, 240×320, SPI) */
#define KAT_LCD_HOST SPI2_HOST
#define KAT_PIN_LCD_MOSI GPIO_NUM_11          /* TODO */
#define KAT_PIN_LCD_SCLK GPIO_NUM_12          /* TODO */
#define KAT_PIN_LCD_CS GPIO_NUM_10            /* TODO */
#define KAT_PIN_LCD_DC GPIO_NUM_13            /* TODO */
#define KAT_PIN_LCD_RST GPIO_NUM_14           /* TODO */
#define KAT_PIN_LCD_BL GPIO_NUM_15            /* TODO */
#define KAT_LCD_HZ (40 * 1000 * 1000)

/* Buzzer (passive, driven by LEDC PWM) */
#define KAT_PIN_BUZZER GPIO_NUM_16            /* TODO */

#endif