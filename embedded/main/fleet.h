/* The Fleet client (KATOO-10): MQTT to AWS IoT Core with this Katooni's own certificate, taking
 * commands on katooni/<thing>/commands and answering on katooni/<thing>/responses. ota.c does the
 * work. (From tinycrypt's notifier.c, less the presence push.) */
#pragma once

#include <stdbool.h>

#include "prov.h"

/* After Wi-Fi is started. prov must stay valid for as long as the client runs. */
bool fleet_start(const prov_t *prov);

/* Queue a JSON response for Fleet; dropped (false) while disconnected. */
bool fleet_respond(const char *json);
