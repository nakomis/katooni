/* Katooni: a cable tester that sorts the drawer. See docs/design.md. */
#include "app_state.h"
#include "buzzer.h"
#include "display.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fusb302.h"
#include "hw_i2c.h"
#include "lines_hw.h"
#include "loadtest.h"
#include "net.h"
#include "nvs_flash.h"
#include "tester.h"

static const char *TAG = "katooni";

static void nvs_up(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    nvs_up();
    app_state_init();
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "Katooni %s", app->version);

    /* Bring up what we can and report what we can't: a board with a missing part should still say so
     * on the screen rather than boot-loop. */
    if (display_init() == ESP_OK) display_splash(app->version);
    if (buzzer_init() != ESP_OK) ESP_LOGW(TAG, "no buzzer");
    bool hw_ok = hw_i2c_init() == ESP_OK && lines_init() == ESP_OK;
    if (!hw_ok) ESP_LOGE(TAG, "line expanders not answering");
    if (fusb302_init() != ESP_OK) ESP_LOGW(TAG, "FUSB302B not answering: no e-marker reads");
    if (loadtest_init() != ESP_OK) ESP_LOGW(TAG, "INA226s not answering: no resistance check");
    app_state_load();
    if (net_start() != ESP_OK) ESP_LOGW(TAG, "no network");

    vTaskDelay(pdMS_TO_TICKS(1500));   /* let the splash be seen */
    tester_start(hw_ok);
}
