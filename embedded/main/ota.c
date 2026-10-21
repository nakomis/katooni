/* OTA through Fleet (KATOO-10). See ota.h. Adapted from tinycrypt's ota.c (CRYPT-14): the download,
 * checks, probation and back-off are the same; the approval wait and the BLE pause are gone. */
#include "ota.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "fleet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "ota_cmd.h"

#define PROJECT "katooni"
#define NVS_NAMESPACE "ota"
#define NVS_TARGET "target"            /* the version an install restarted into */
#define NVS_TARGET_SLOT "target_slot"  /* and its slot (a reinstall of the same version rolls back too) */
#define READ_CHUNK 4096
#define HTTP_TIMEOUT_MS 15000
#define RESTART_DELAY_MS 2000          /* for "installing" to reach IoT Core first */

extern const char root_ca_start[] asm("_binary_AmazonRootCA1_pem_start");

static const char *TAG = "ota";

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_busy, s_cancel;
static TaskHandle_t s_task;

/* The update being worked on: written by the MQTT task while idle, then owned by the OTA task. */
static char *s_url;
static char s_version[OTA_VERSION_MAX];
static uint8_t s_sha256[OTA_SHA256_LEN];
/* SHA-256 of the last update's URL. IoT Core redelivers the very same bytes when a PUBACK is slow;
 * every trigger from Fleet presigns a fresh URL. So the same URL again is a redelivery, never a retry. */
static uint8_t s_last_url_hash[32];

static int64_t s_retry_after_ms;   /* no update starts before this (back-off) */
static uint32_t s_backoff_s;

static nvs_handle_t s_nvs;
static bool s_nvs_ok;
static volatile bool s_probation;
static esp_timer_handle_t s_verify_deadline;

static const char *running_version(void)
{
    return esp_app_get_description()->version;
}

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

/* ---- responses ---------------------------------------------------------------------------- */

static void respond(cJSON *msg)
{
    char *text = cJSON_PrintUnformatted(msg);
    if (text) fleet_respond(text);
    cJSON_free(text);
    cJSON_Delete(msg);
}

static cJSON *message(const char *type)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", type);
    return msg;
}

static void respond_progress(const char *status, int progress, const char *version)
{
    cJSON *msg = message("ota_progress");
    cJSON_AddStringToObject(msg, "status", status);
    cJSON_AddNumberToObject(msg, "progress", progress);
    if (version) cJSON_AddStringToObject(msg, "version", version);
    respond(msg);
}

static void respond_error(const char *type, const char *text, const char *version)
{
    ESP_LOGW(TAG, "%s: %s", type, text);
    cJSON *msg = message(type);
    cJSON_AddStringToObject(msg, "message", text);
    if (version) cJSON_AddStringToObject(msg, "version", version);
    respond(msg);
}

static void respond_version(void)
{
    cJSON *msg = message("version");
    cJSON_AddStringToObject(msg, "version", running_version());
    cJSON_AddStringToObject(msg, "project", PROJECT);
    respond(msg);
}

/* ---- boot: probation and rollback --------------------------------------------------------- */

static void verify_deadline_fire(void *arg)
{
    if (!s_probation) return;
    ESP_LOGE(TAG, "new image didn't reach IoT Core within %d s: rolling back", OTA_VERIFY_DEADLINE_S);
    esp_ota_mark_app_invalid_rollback_and_reboot();
    abort();
}

void ota_boot(bool local_selftest_ok, bool fleet)
{
    s_nvs_ok = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs) == ESP_OK;
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    bool pending = esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
    ESP_LOGI(TAG, "running %s %s from %s%s", PROJECT, running_version(), running->label,
             pending ? " (new image, on probation)" : "");
    if (!pending) return;
    if (!local_selftest_ok) {
        ESP_LOGE(TAG, "new image failed its self-test: rolling back");
        esp_ota_mark_app_invalid_rollback_and_reboot();
        abort();   /* only if that failed: a reset of a pending image rolls back too */
    }
    if (!fleet) {
        esp_ota_mark_app_valid_cancel_rollback();
        return;
    }
    /* Kept only once it can be updated again (ota_on_connected). */
    s_probation = true;
    const esp_timer_create_args_t args = {.callback = verify_deadline_fire, .name = "ota_verify"};
    if (esp_timer_create(&args, &s_verify_deadline) != ESP_OK ||
        esp_timer_start_once(s_verify_deadline, (uint64_t)OTA_VERIFY_DEADLINE_S * 1000000) != ESP_OK) {
        ESP_LOGE(TAG, "no probation timer: rolling back");
        esp_ota_mark_app_invalid_rollback_and_reboot();
        abort();
    }
}

void ota_on_connected(void)
{
    if (s_probation) {
        s_probation = false;
        esp_timer_stop(s_verify_deadline);
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "new image passed its self-test: kept (%s)", esp_err_to_name(err));
    }
    respond_version();

    /* The outcome of the last install, reported once. */
    char target[OTA_VERSION_MAX], slot[17] = "";
    size_t len = sizeof target, slot_len = sizeof slot;
    if (!s_nvs_ok || nvs_get_str(s_nvs, NVS_TARGET, target, &len) != ESP_OK) return;
    bool slot_known = nvs_get_str(s_nvs, NVS_TARGET_SLOT, slot, &slot_len) == ESP_OK;
    if (strcmp(target, running_version()) == 0 &&
        (!slot_known || strcmp(slot, esp_ota_get_running_partition()->label) == 0)) {
        cJSON *msg = message("ota_complete");
        cJSON_AddStringToObject(msg, "version", target);
        respond(msg);
    } else {
        respond_error("ota_error", "rolled back", target);
    }
    nvs_erase_key(s_nvs, NVS_TARGET);
    nvs_erase_key(s_nvs, NVS_TARGET_SLOT);
    nvs_commit(s_nvs);
}

/* ---- download and checks ------------------------------------------------------------------ */

/* "<what> (tls ... errno ... heap free/largest)": a failure says why itself, since the log is
 * usually out of reach. */
static const char *explain(esp_http_client_handle_t http, const char *what, char *why, size_t why_len)
{
    int tls_code = 0, tls_flags = 0;
    esp_err_t tls = esp_http_client_get_and_clear_last_tls_error(http, &tls_code, &tls_flags);
    snprintf(why, why_len, "download failed: %s (tls %s -0x%x flags 0x%x, errno %d, heap %u/%u)", what,
             esp_err_to_name(tls), (unsigned)-tls_code, (unsigned)tls_flags, esp_http_client_get_errno(http),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    ESP_LOGE(TAG, "%s", why);
    return why;
}

/* Returns NULL on success, else the reason (for ota_error). */
static const char *download(const esp_partition_t *slot, char *why, size_t why_len)
{
    const char *err_text = NULL;
    int64_t started = now_ms();
    uint8_t *buf = malloc(READ_CHUNK);
    esp_http_client_config_t config = {
        .url = s_url,
        .cert_pem = root_ca_start,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .keep_alive_enable = false,
        /* The request line carries the whole presigned URL (~1.5-3 KB with an STS token); the default
         * 512-byte TX buffer can't hold it. */
        .buffer_size_tx = (int)strlen(s_url) + 1024,
    };
    esp_http_client_handle_t http = buf ? esp_http_client_init(&config) : NULL;
    esp_ota_handle_t ota = 0;
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    if (!http) {
        err_text = "download failed: out of memory";
        goto out;
    }
    esp_err_t err = esp_http_client_open(http, 0);
    if (err != ESP_OK) {
        err_text = explain(http, esp_err_to_name(err), why, why_len);
        goto out;
    }
    int64_t length = esp_http_client_fetch_headers(http);
    int status = esp_http_client_get_status_code(http);
    ESP_LOGI(TAG, "HTTP %d, %" PRId64 " bytes", status, length);
    if (status != 200) {
        snprintf(why, why_len, "download failed: HTTP %d", status);
        err_text = why;
        goto out;
    }
    if (length > (int64_t)slot->size) {
        err_text = "image too large";
        goto out;
    }
    err = esp_ota_begin(slot, OTA_WITH_SEQUENTIAL_WRITES, &ota);
    if (err != ESP_OK) {
        snprintf(why, why_len, "download failed: %s", esp_err_to_name(err));
        err_text = why;
        ota = 0;
        goto out;
    }
    mbedtls_sha256_starts(&sha, 0);
    size_t total = 0, window_total = 0;
    int last_tenth = -1, stalls = 0;
    int64_t window_start = now_ms();
    for (;;) {
        if (s_cancel) {
            err_text = "cancelled";
            goto out;
        }
        int64_t now = now_ms();
        if (now - started > OTA_DOWNLOAD_DEADLINE_MS) {
            snprintf(why, why_len, "download failed: over %d s", CONFIG_KATOONI_OTA_DOWNLOAD_DEADLINE_S);
            err_text = why;
            goto out;
        }
        if (now - window_start >= OTA_MIN_RATE_WINDOW_MS) {
            if (total - window_total < OTA_MIN_RATE_BYTES) {
                snprintf(why, why_len, "download failed: too slow (%u bytes in %d s)",
                         (unsigned)(total - window_total), OTA_MIN_RATE_WINDOW_MS / 1000);
                err_text = why;
                goto out;
            }
            window_start = now;
            window_total = total;
        }
        int n = esp_http_client_read(http, (char *)buf, READ_CHUNK);
        if (n == -ESP_ERR_HTTP_EAGAIN && ++stalls <= 3) continue;
        if (n < 0) {
            char what[48];
            snprintf(what, sizeof what, "read %d after %u bytes", n, (unsigned)total);
            err_text = explain(http, what, why, why_len);
            goto out;
        }
        stalls = 0;
        if (n == 0) {
            if (!esp_http_client_is_complete_data_received(http)) {
                err_text = "download failed: connection closed early";
                goto out;
            }
            break;
        }
        total += (size_t)n;
        if (total > slot->size) {
            err_text = "image too large";
            goto out;
        }
        mbedtls_sha256_update(&sha, buf, (size_t)n);
        err = esp_ota_write(ota, buf, (size_t)n);
        if (err != ESP_OK) {
            snprintf(why, why_len, "download failed: %s", esp_err_to_name(err));
            err_text = why;
            goto out;
        }
        if (length > 0 && (int)(total * 10 / (size_t)length) != last_tenth) {
            last_tenth = (int)(total * 10 / (size_t)length);
            respond_progress("downloading", last_tenth * 10, s_version);
        }
    }

    uint8_t got[OTA_SHA256_LEN];
    mbedtls_sha256_finish(&sha, got);
    if (memcmp(got, s_sha256, sizeof got) != 0) {
        err_text = "sha256 mismatch";
        goto out;
    }
    /* Checks the image's own integrity and its signature against the running app's signing key. */
    err = esp_ota_end(ota);
    ota = 0;
    if (err != ESP_OK) {
        err_text = err == ESP_ERR_OTA_VALIDATE_FAILED ? "bad signature" : "verify failed";
        goto out;
    }
    esp_app_desc_t desc;
    if (esp_ota_get_partition_description(slot, &desc) != ESP_OK || strcmp(desc.project_name, PROJECT) != 0 ||
        strcmp(desc.version, s_version) != 0)
        err_text = "wrong image";
    else if (!ota_version_valid(running_version()))
        err_text = "running version unknown";   /* so no downgrade check is possible */
    else if (ota_version_cmp(s_version, running_version()) < 0)
        err_text = "downgrade refused";
    else if (ota_version_cmp(s_version, running_version()) == 0)
        err_text = "already running";

out:
    if (ota) esp_ota_abort(ota);
    mbedtls_sha256_free(&sha);
    if (http) esp_http_client_cleanup(http);
    free(buf);
    return err_text;
}

static void finish(void)
{
    char *url = s_url;
    s_url = NULL;
    free(url);
    taskENTER_CRITICAL(&s_lock);
    s_busy = false;
    s_cancel = false;
    taskEXIT_CRITICAL(&s_lock);
}

static void ota_task(void *arg)
{
    static char why[160];
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const esp_partition_t *slot = esp_ota_get_next_update_partition(NULL);
        ESP_LOGI(TAG, "update to %s: downloading into %s", s_version, slot ? slot->label : "(none)");
        const char *failed = slot ? download(slot, why, sizeof why) : "no update slot";
        taskENTER_CRITICAL(&s_lock);
        if (failed) {
            /* Every failure here erased the spare slot, a cancel included: back off. */
            s_backoff_s = s_backoff_s == 0                      ? OTA_BACKOFF_FIRST_S
                          : s_backoff_s >= OTA_BACKOFF_MAX_S / 2 ? OTA_BACKOFF_MAX_S
                                                                 : s_backoff_s * 2;
            s_retry_after_ms = now_ms() + (int64_t)s_backoff_s * 1000;
        } else {
            s_backoff_s = 0;
        }
        taskEXIT_CRITICAL(&s_lock);
        if (failed) {
            if (strcmp(failed, "cancelled") == 0) respond_progress("Update cancelled", 0, NULL);
            else respond_error("ota_error", failed, s_version);
            finish();
            continue;
        }

        esp_err_t err = s_nvs_ok ? nvs_set_str(s_nvs, NVS_TARGET, s_version) : ESP_ERR_INVALID_STATE;
        if (err == ESP_OK) err = nvs_set_str(s_nvs, NVS_TARGET_SLOT, slot->label);
        if (err == ESP_OK) err = nvs_commit(s_nvs);
        if (err == ESP_OK) err = esp_ota_set_boot_partition(slot);   /* checks the image (and signature) again */
        if (err != ESP_OK) {
            if (s_nvs_ok) {
                nvs_erase_key(s_nvs, NVS_TARGET);
                nvs_erase_key(s_nvs, NVS_TARGET_SLOT);
                nvs_commit(s_nvs);
            }
            respond_error("ota_error", "install failed", s_version);
            finish();
            continue;
        }
        respond_progress("installing", 100, s_version);
        vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
        ESP_LOGI(TAG, "restarting into %s", s_version);
        esp_restart();
    }
}

/* ---- commands (MQTT task) ------------------------------------------------------------------ */

static void start_update(const ota_cmd *cmd)
{
    if (!s_task && xTaskCreate(ota_task, "ota", 8 * 1024, NULL, 4, &s_task) != pdPASS) {
        s_task = NULL;
        respond_error("ota_error", "out of memory", cmd->version);
        return;
    }
    uint8_t url_hash[32];
    mbedtls_sha256((const unsigned char *)cmd->url, strlen(cmd->url), url_hash, 0);
    if (memcmp(url_hash, s_last_url_hash, sizeof url_hash) == 0) {
        /* No reply, on purpose: a redelivery usually lands during the very download it started, and
         * an ota_error with that version would show Fleet a running update as failed. */
        ESP_LOGI(TAG, "update to %s with the same URL as the last: a redelivery, ignored", cmd->version);
        return;
    }
    if (ota_version_valid(running_version()) && ota_version_cmp(cmd->version, running_version()) <= 0) {
        respond_error("ota_error",
                      ota_version_cmp(cmd->version, running_version()) == 0 ? "already running" : "downgrade refused",
                      cmd->version);
        return;
    }
    char *url = strdup(cmd->url);
    if (!url) {
        respond_error("ota_error", "out of memory", cmd->version);
        return;
    }
    int64_t now = now_ms(), backoff_until = 0;
    taskENTER_CRITICAL(&s_lock);
    bool busy = s_busy;
    if (!busy && now < s_retry_after_ms) backoff_until = s_retry_after_ms;
    else if (!busy) {
        s_busy = true;
        s_cancel = false;
    }
    taskEXIT_CRITICAL(&s_lock);
    if (busy || backoff_until) {
        char why[48];
        if (busy) snprintf(why, sizeof why, "busy");
        else snprintf(why, sizeof why, "busy: retry in %u s", (unsigned)((backoff_until - now + 999) / 1000));
        free(url);
        respond_error("ota_error", why, cmd->version);
        return;
    }
    memcpy(s_last_url_hash, url_hash, sizeof s_last_url_hash);
    s_url = url;
    snprintf(s_version, sizeof s_version, "%s", cmd->version);
    memcpy(s_sha256, cmd->sha256, sizeof s_sha256);
    xTaskNotifyGive(s_task);
}

void ota_handle_command(const char *data, size_t len)
{
    static ota_chunks chunks;
    static ota_cmd cmd;   /* 8 KB: not on the MQTT task's stack */
    ota_cmd_parse(&chunks, data, len, (uint32_t)(esp_timer_get_time() / 1000), &cmd);
    switch (cmd.kind) {
    case OTA_CMD_GET_VERSION:
        respond_version();
        break;
    case OTA_CMD_PING: {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        cJSON *msg = message("pong");
        cJSON_AddNumberToObject(msg, "timestamp", (double)tv.tv_sec * 1000 + tv.tv_usec / 1000);
        respond(msg);
        break;
    }
    case OTA_CMD_CANCEL:
        if (s_busy) s_cancel = true;
        else respond_progress("Update cancelled", 0, NULL);
        break;
    case OTA_CMD_CHUNK: {
        char status[48];
        snprintf(status, sizeof status, "Chunk %d/%d received", cmd.chunk_index + 1, cmd.total_chunks);
        respond_progress(status, 0, NULL);   /* no version: an ack can't reopen a finished update */
        break;
    }
    case OTA_CMD_UPDATE:
        ESP_LOGI(TAG, "update to %s requested", cmd.version);
        start_update(&cmd);
        break;
    case OTA_CMD_ERROR:
        respond_error(cmd.ota_error ? "ota_error" : "error", cmd.message, NULL);
        break;
    }
}

bool ota_busy(void)
{
    return s_busy;
}
