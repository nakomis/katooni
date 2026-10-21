/* This Katooni's own settings and IoT credentials (KATOO-10), read from the `prov` NVS partition that
 * infra/scripts/provision-device.sh writes. Nothing here is in the firmware image, so every Katooni
 * runs the same image and a published update holds no secrets. (From tinycrypt's prov.c.)
 *
 * The device private key sits in plain flash: fine for a cable tester, whose certificate only lets it
 * take its own Fleet commands and answer them. */
#pragma once

#include <stdbool.h>

typedef struct {
    char wifi_ssid[33];
    char wifi_password[65];
    char iot_endpoint[128];
    char thing_name[129];   /* also the MQTT client id */
    char *device_cert;      /* PEM, malloc'd */
    char *device_key;       /* PEM, malloc'd */
} prov_t;

/* Reads the partition. False (and a log line saying what's missing) if this Katooni isn't
 * provisioned; then Wi-Fi falls back to sdkconfig.secrets and there's no Fleet. */
bool prov_load(prov_t *prov);
