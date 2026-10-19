/* Over-the-air updates through Fleet (KATOO-10), with Katooni as the trust anchor. From tinycrypt's
 * ota.c (CRYPT-14), without the watch approval and the BLE pause.
 *
 * Fleet only says "please update": ota_update {url, version, sha256} (or url_chunks) on
 * katooni/<thing>/commands. Katooni then
 *   1. downloads the image over HTTPS (Amazon Root CA 1) into the spare slot, hashing as it goes;
 *   2. refuses it unless its SHA-256 is Fleet's, its RSA-3072 signature is by the key that signed the
 *      running app (esp_ota_end, CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT), it says it is
 *      katooni at that version, and that version is newer than the running one;
 *   3. switches slots and restarts.
 * The new image runs a self-test and must reach IoT Core within OTA_VERIFY_DEADLINE_S; until then
 * it's on probation, and a failure or crash rolls back to the old one, which reports it. Progress
 * goes back on katooni/<thing>/responses as Fleet expects. */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "sdkconfig.h"

#define OTA_VERIFY_DEADLINE_S 120

/* What a misbehaving Fleet (or anyone who can publish to the commands topic) can cost: a download
 * erases the spare slot. Kept in RAM only: a reboot starts afresh. */
#define OTA_DOWNLOAD_DEADLINE_MS (CONFIG_KATOONI_OTA_DOWNLOAD_DEADLINE_S * 1000)
#define OTA_MIN_RATE_WINDOW_MS 30000   /* at least OTA_MIN_RATE_BYTES in every window, or abandon it */
#define OTA_MIN_RATE_BYTES (32 * 1024)
#define OTA_BACKOFF_FIRST_S 30         /* after a failed download; doubles, up to OTA_BACKOFF_MAX_S */
#define OTA_BACKOFF_MAX_S 900

/* At boot, after NVS is up, with the result of the local self-test. A new image that fails it is
 * rolled back at once. fleet: this Katooni is provisioned for Fleet; if not, a new image can't prove
 * it reaches IoT Core, so it's kept straight away. */
void ota_boot(bool local_selftest_ok, bool fleet);

/* From the Fleet client (MQTT task): subscribed to commands, so this image can be updated again and
 * may be kept; reports the version and the outcome of the last update. */
void ota_on_connected(void);

/* From the Fleet client (MQTT task): one command. */
void ota_handle_command(const char *data, size_t len);

bool ota_busy(void);
