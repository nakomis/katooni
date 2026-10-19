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
 * - The ST7789 (240×320) is on SPI2 with its backlight switched by KAT_PIN_LCD_BL. */
#pragma once

#include "driver/gpio.h"

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
