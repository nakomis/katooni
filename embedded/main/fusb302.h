/* FUSB302B on the A-end USB-C: power the cable's e-marker through VCONN and ask it who it is. */
#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t fusb302_init(void);

/* cc: which of the A end's CC pins carries the cable's CC wire (1 or 2; VCONN goes on the other).
 * objs: room for 7 data objects. Returns the number received (0: no e-marker answered), or -1 on an
 * I²C error. Needs the A-end GND switched to board ground first. */
int fusb302_discover_identity(int cc, uint32_t objs[7]);
