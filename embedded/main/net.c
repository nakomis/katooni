#include "net.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "sdkconfig.h"

/* After this many failed joins in a row, bring up the access point as well (and keep trying). */
#define FAILS_BEFORE_AP 5
#define RETRY_US (5 * 1000 * 1000)

static const char *TAG = "net";
static esp_netif_t *s_sta, *s_ap;
static esp_timer_handle_t s_retry;
static int s_fails;
static bool s_ap_on, s_have_ip;
static char s_ip[16];

static void start_ap(void)
{
    if (s_ap_on) return;
    wifi_config_t ap = {
        .ap = {
            .ssid = "katooni",
            .ssid_len = 7,
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    if (strlen(CONFIG_KATOONI_AP_PASSWORD) >= 8) {
        strlcpy((char *)ap.ap.password, CONFIG_KATOONI_AP_PASSWORD, sizeof ap.ap.password);
        ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }
    wifi_mode_t mode = strlen(CONFIG_KATOONI_WIFI_SSID) ? WIFI_MODE_APSTA : WIFI_MODE_AP;
    esp_wifi_set_mode(mode);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    s_ap_on = true;
    ESP_LOGI(TAG, "access point \"katooni\" up at 192.168.4.1");
}

static void retry(void *arg)
{
    esp_wifi_connect();
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_have_ip = false;
        if (++s_fails >= FAILS_BEFORE_AP) start_ap();
        esp_timer_start_once(s_retry, RETRY_US);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        snprintf(s_ip, sizeof s_ip, IPSTR, IP2STR(&e->ip_info.ip));
        s_have_ip = true;
        s_fails = 0;
        ESP_LOGI(TAG, "joined %s as %s", CONFIG_KATOONI_WIFI_SSID, s_ip);
    }
}

esp_err_t net_start(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    s_sta = esp_netif_create_default_wifi_sta();
    s_ap = esp_netif_create_default_wifi_ap();
    esp_netif_set_hostname(s_sta, CONFIG_KATOONI_HOSTNAME);

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi");
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL);
    esp_timer_create_args_t t = {.callback = retry, .name = "wifi-retry"};
    ESP_RETURN_ON_ERROR(esp_timer_create(&t, &s_retry), TAG, "timer");

    if (strlen(CONFIG_KATOONI_WIFI_SSID)) {
        wifi_config_t sta = {0};
        strlcpy((char *)sta.sta.ssid, CONFIG_KATOONI_WIFI_SSID, sizeof sta.sta.ssid);
        strlcpy((char *)sta.sta.password, CONFIG_KATOONI_WIFI_PASSWORD, sizeof sta.sta.password);
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &sta);
    } else {
        ESP_LOGW(TAG, "no network configured (sdkconfig.secrets): access point only");
        start_ap();
    }
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");

    ESP_RETURN_ON_ERROR(mdns_init(), TAG, "mdns");
    mdns_hostname_set(CONFIG_KATOONI_HOSTNAME);
    mdns_instance_name_set("Katooni cable tester");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    return ESP_OK;
}

void net_where(char *buf, size_t n)
{
    if (s_have_ip) snprintf(buf, n, "http://%s.local (%s)", CONFIG_KATOONI_HOSTNAME, s_ip);
    else if (s_ap_on) snprintf(buf, n, "Wi-Fi \"katooni\", then http://192.168.4.1");
    else snprintf(buf, n, "joining %s...", CONFIG_KATOONI_WIFI_SSID);
}
