/* Katooni's pin maps: the PCB (PLACEHOLDER VALUES) and the breadboard prototype.
 *
 * TODO(KATOO-1): the board generator (hardware/board/build.py) writes the real pins.h. Replace every
 * PCB value marked TODO from it once the board is routed, keeping these names (or rename here and
 * in the drivers together). Nothing in the PCB profile has been checked against a schematic.
 *
 * What the firmware assumes about the circuit:
 * - One I²C bus: the line expanders, the FUSB302B (fixed at 0x22, so no expander may use 0x22) and
 *   two INA226s.
 * - Every connector pin is its own expander line, through a series resistor; none is tied to board
 *   ground. Pull-ups (~100 kΩ, in the expanders) hold every line high; a scan drives one line low and
 *   reads which others follow.
 * - Expanders are MCP23017s or TCA9555s (KAT_EXPANDER_TYPE). **On an MCP23017, GPA7 and GPB7 are
 *   output-only** (Microchip's current datasheets), so no line may sit on bit 7 or 15: 14 lines per
 *   chip. kat_line_map() is the only place lines meet pins, so either part works.
 * - KAT_PIN_A_GND_EN switches the A-end USB-C's GND to board ground for the PD and load tests;
 *   KAT_PIN_LOAD_EN switches the test current into the A-end VBUS; KAT_PIN_FAR_SHORT shorts the
 *   B-end USB-C's VBUS to its GND to close the loop. All three are active high and off at reset.
 * - The ST7789 (240×320) is on SPI2 with its backlight switched by KAT_PIN_LCD_BL (GPIO_NUM_NC
 *   for a module whose backlight is always on). Inversion and offsets are in Kconfig.
 * - CONFIG_KATOONI_BOARD_BREADBOARD picks the breadboard profile instead. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "katooni/lines.h"
#include "sdkconfig.h"

#define KAT_EXPANDER_MCP23017 1
#define KAT_EXPANDER_TCA9555 2

/* Usable MCP23017 pins in order: GPA0-6, then GPB0-6 (bits 8-14). Never GPA7/GPB7. */
static inline uint8_t kat_mcp23017_bit(int i)
{
    return (uint8_t)(i < 7 ? i : i + 1);
}

#if CONFIG_KATOONI_BOARD_BREADBOARD
/* Breadboard prototype: an ESP32-S3-DevKitC with an Estardyn GMT020-02 (2.0" ST7789V, 240×320, SPI:
 * CS, DC, RST, SDA = MOSI, SCL = SCK, VCC, GND; no MISO, backlight always on). The pins avoid the S3's
 * strapping pins (0, 3, 45, 46), native USB (19, 20) and the DevKitC's RGB LED (48). Whatever isn't
 * wired up is reported at boot and skipped.
 *
 * Stage 1 is RJ45 to RJ45: 18 lines on two MCP23017 DIP-28s, A-end jack (pins 1-8, shield) on the
 * one at 0x20 and B-end jack on 0x21, each on GPA0-6 then GPB0-1. Every other line is unmapped, so
 * USB cables read as "no cable" until more expanders are added. */
#define KAT_PIN_I2C_SDA GPIO_NUM_1
#define KAT_PIN_I2C_SCL GPIO_NUM_2
#define KAT_I2C_HZ 400000
#define KAT_EXPANDER_TYPE KAT_EXPANDER_MCP23017
#define KAT_EXPANDER_COUNT 2
#define KAT_EXPANDER_ADDRS {0x20, 0x21}

static inline bool kat_line_map(int line, uint8_t *exp, uint8_t *bit)
{
    kat_port_t port = kat_line_port(line);
    if (port != KAT_PORT_A_RJ45 && port != KAT_PORT_B_RJ45) return false;
    *exp = port == KAT_PORT_A_RJ45 ? 0 : 1;
    *bit = kat_mcp23017_bit(kat_line_pin(line));
    return true;
}

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

/* Expanders. TODO: the part and addresses from the board (KATOO-1). Placeholder: six MCP23017s,
 * 14 usable pins each (84 for 71 lines), lines in order. */
#define KAT_EXPANDER_TYPE KAT_EXPANDER_MCP23017
#define KAT_EXPANDER_COUNT 6
#define KAT_EXPANDER_ADDRS {0x20, 0x21, 0x23, 0x24, 0x25, 0x26}   /* never 0x22 (the FUSB302B) */

/* Which expander pin a conductor line (katooni/lines.h numbering) is on: bit 0-7 = port A (or
 * TCA9555 port 0), 8-15 = port B (port 1). False for a line with no pin. TODO: from the routed board. */
static inline bool kat_line_map(int line, uint8_t *exp, uint8_t *bit)
{
    if (line < 0 || line >= KAT_LINE_COUNT) return false;
    *exp = (uint8_t)(line / 14);
    *bit = kat_mcp23017_bit(line % 14);
    return true;
}

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
