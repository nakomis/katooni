/* Katooni: a cable tester that sorts the drawer. See docs/design.md. */
#include "app_state.h"
#include "buzzer.h"
#include "display.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "fleet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fusb302.h"
#include "hw_i2c.h"
#include "katooni/simcable.h"
#include "lines_hw.h"
#include "loadtest.h"
#include "net.h"
#include "nvs_flash.h"
#include "ota.h"
#include "prov.h"
#include "sdkconfig.h"
#include "tester.h"
#include "web.h"

static const char *TAG = "katooni";
static prov_t s_prov;   /* lives as long as the Fleet client */

static bool nvs_up(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    return err == ESP_OK;
}

/* What a freshly updated image must manage before it's kept: NVS, and the core classifying a known
 * cable correctly. No hardware: a board with a missing part must still be updatable. */
static bool selftest(bool nvs_ok)
{
#if CONFIG_KATOONI_OTA_FAIL_SELFTEST
    ESP_LOGE(TAG, "TEST BUILD: failing the self-test on purpose");
    return false;
#endif
    kat_scan_t scan;
    kat_continuity_t cont;
    kat_sim_usb_t cable = {.kind = KAT_KIND_C_C, .flip_b = 1, .ra = true};
    kat_sim_usb(&scan, &cable);
    kat_classify(&scan, &cont);
    return nvs_ok && cont.cls == KAT_CLASS_SUPERSPEED && cont.fault_count == 0;
}

void app_main(void)
{
    bool nvs_ok = nvs_up();
    app_state_init();
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "Katooni %s", app->version);
    bool provisioned = prov_load(&s_prov), fleet = provisioned && CONFIG_KATOONI_FLEET;
    ota_boot(selftest(nvs_ok), fleet);

    /* Bring up what we can and report what we can't: a board with a missing part should still say so
     * on the screen rather than boot-loop. */
    if (display_init() == ESP_OK) display_splash(app->version);
    if (buzzer_init() != ESP_OK) ESP_LOGW(TAG, "no buzzer");
    bool hw_ok = hw_i2c_init() == ESP_OK && lines_init() == ESP_OK;
    if (!hw_ok) ESP_LOGE(TAG, "line expanders not answering");
    if (fusb302_init() != ESP_OK) ESP_LOGW(TAG, "FUSB302B not answering: no e-marker reads");
    if (loadtest_init() != ESP_OK) ESP_LOGW(TAG, "INA226s not answering: no resistance check");
    app_state_load();
    if (net_start(provisioned ? s_prov.wifi_ssid : NULL, provisioned ? s_prov.wifi_password : NULL) != ESP_OK) {
        ESP_LOGW(TAG, "no network");
    } else {
        if (web_start() != ESP_OK) ESP_LOGW(TAG, "no web page");
        if (fleet) fleet_start(&s_prov);
    }

    vTaskDelay(pdMS_TO_TICKS(1500));   /* let the splash be seen */
    tester_start(hw_ok);
}
