/* Wi-Fi: join the network from the prov partition (or, for development builds, sdkconfig.secrets),
 * or run a "katooni" access point if that fails; either way, answer to katooni.local. */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

/* ssid/password: from the prov partition, or NULL to use sdkconfig.secrets'. */
esp_err_t net_start(const char *ssid, const char *password);

/* Where to point a browser, e.g. "http://katooni.local (192.168.1.23)" or "Wi-Fi katooni,
 * http://192.168.4.1". Empty until there's a network. */
void net_where(char *buf, size_t n);
