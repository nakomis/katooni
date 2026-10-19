/* The continuity scan on the real hardware: five MCP23017s, one line per connector pin. */
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "katooni/scan.h"

esp_err_t lines_init(void);

/* Drive each line low in turn and record every line that follows it. stuck_low counts lines that
 * were already low with nothing driven (a board fault, or something powered on a connector). */
esp_err_t lines_scan(kat_scan_t *scan, int *stuck_low);

/* Pull-ups off on every line (true) or back on (false): the PD and load tests run with the lines
 * quiet, so the expanders don't load CC or VCONN. */
esp_err_t lines_quiet(bool quiet);
