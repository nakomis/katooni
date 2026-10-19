/* The loop resistance measurement (KATOO-4): current round VBUS and GND, Kelvin-sensed.
 *
 * PROVISIONAL: written against the arrangement assumed in board_pins.h, which the board stream
 * (KATOO-1) hasn't confirmed. The test current flows from the board's 5 V through INA226 #1's shunt
 * and a current limit into the A-end VBUS, through the cable to the B end, across the far-end short
 * (KAT_PIN_FAR_SHORT) into the B-end GND, and back through the cable's GND to the A end and board
 * ground (KAT_PIN_A_GND_EN). INA226 #1's bus voltage is the A-end VBUS; INA226 #2's shunt inputs sit
 * across the far-end short, so its shunt voltage is the drop that isn't the cable's:
 *
 *     loop drop = V(A-end VBUS) − V(far-end short)          current = shunt voltage / shunt */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t loadtest_init(void);

/* Switch the load on for long enough to read, then off. Only call once the continuity scan has
 * shown VBUS and GND are continuous and not shorted to each other. */
esp_err_t loadtest_measure(int32_t *drop_uv, int32_t *current_ua);

/* Switch the A-end GND to board ground (for the PD test as well), or release it. */
void loadtest_ground_a_end(bool on);
