/* The session web page (KATOO-7): web/index.html plus a small JSON API, on port 80.
 *   GET  /                  the page
 *   GET  /api/session       every bag and the latest faulty cables (katooni/json.h)
 *   GET  /api/last          the last cable in full, with a sequence number to poll
 *   POST /api/session/new   clear the session (the page asks for two taps first) */
#pragma once

#include "esp_err.h"

esp_err_t web_start(void);
