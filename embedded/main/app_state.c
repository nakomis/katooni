#include "app_state.h"

#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define NS "katooni"
#define KEY "session"

static const char *TAG = "state";
static app_state_t s_state;
static SemaphoreHandle_t s_lock;

void app_state_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    kat_session_clear(&s_state.session);
    kat_config_default(&s_state.config);
}

app_state_t *app_state_lock(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    return &s_state;
}

void app_state_unlock(void)
{
    xSemaphoreGive(s_lock);
}

void app_state_save(void)
{
    size_t n = kat_session_blob_size();
    uint8_t *buf = malloc(n);
    nvs_handle_t h;
    if (!buf) return;
    if (kat_session_save(&s_state.session, buf, n) && nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        esp_err_t err = nvs_set_blob(h, KEY, buf, n);
        if (err == ESP_OK) err = nvs_commit(h);
        if (err != ESP_OK) ESP_LOGW(TAG, "save failed: %s", esp_err_to_name(err));
        nvs_close(h);
    }
    free(buf);
}

bool app_state_load(void)
{
    size_t n = kat_session_blob_size();
    uint8_t *buf = malloc(n);
    nvs_handle_t h;
    bool ok = false;
    if (!buf) return false;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = n;
        if (nvs_get_blob(h, KEY, buf, &len) == ESP_OK) {
            app_state_lock();
            ok = kat_session_load(&s_state.session, buf, len);
            app_state_unlock();
        }
        nvs_close(h);
    }
    free(buf);
    if (ok) ESP_LOGI(TAG, "session restored: %u bags", s_state.session.bag_count);
    else ESP_LOGI(TAG, "new session");
    return ok;
}
