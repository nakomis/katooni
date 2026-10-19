#include "fleet.h"

#include <stdio.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "mqtt_client.h"
#include "ota.h"

/* Fleet's commands can carry a presigned URL of ~1.5 KB (longer ones arrive in chunks). */
#define MQTT_BUFFER_SIZE 4096

extern const char root_ca_start[] asm("_binary_AmazonRootCA1_pem_start");

static const char *TAG = "fleet";
static const prov_t *s_prov;
static char s_commands[sizeof(((prov_t *)0)->thing_name) + 32];
static char s_responses[sizeof(((prov_t *)0)->thing_name) + 32];
static int s_subscribe_id = -1;
static esp_mqtt_client_handle_t s_mqtt;
static volatile bool s_connected;

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t e = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "connected to IoT Core as %s", s_prov->thing_name);
        s_subscribe_id = esp_mqtt_client_subscribe(s_mqtt, s_commands, 1);   /* clean session: every time */
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        if (e->msg_id != s_subscribe_id) break;
        if (e->error_handle->error_type == MQTT_ERROR_TYPE_SUBSCRIBE_FAILED) {
            ESP_LOGE(TAG, "subscription to %s refused", s_commands);
        } else {
            ESP_LOGI(TAG, "listening for Fleet on %s", s_commands);
            ota_on_connected();
        }
        break;
    case MQTT_EVENT_DATA:
        if (e->current_data_offset != 0 || e->data_len != e->total_data_len) {
            ESP_LOGW(TAG, "command of %d bytes doesn't fit the %d-byte buffer", e->total_data_len, MQTT_BUFFER_SIZE);
            if (e->current_data_offset == 0) fleet_respond("{\"type\":\"error\",\"message\":\"Command too long\"}");
            break;
        }
        ota_handle_command(e->data, (size_t)e->data_len);
        break;
    case MQTT_EVENT_ERROR:
        if (e->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
            ESP_LOGW(TAG, "transport error: tls 0x%x, stack 0x%x, errno %d", e->error_handle->esp_tls_last_esp_err,
                     e->error_handle->esp_tls_stack_err, e->error_handle->esp_transport_sock_errno);
        else
            ESP_LOGW(TAG, "error type %d, connect return code %d", e->error_handle->error_type,
                     e->error_handle->connect_return_code);
        break;
    default:
        break;
    }
}

bool fleet_start(const prov_t *prov)
{
    s_prov = prov;
    snprintf(s_commands, sizeof s_commands, "katooni/%s/commands", prov->thing_name);
    snprintf(s_responses, sizeof s_responses, "katooni/%s/responses", prov->thing_name);

    /* TLS needs the time to check certificates. */
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sntp);

    static char uri[sizeof prov->iot_endpoint + 16];
    snprintf(uri, sizeof uri, "mqtts://%s:8883", prov->iot_endpoint);
    const esp_mqtt_client_config_t config = {
        .broker = {.address.uri = uri, .verification.certificate = root_ca_start},
        .credentials = {
            .client_id = prov->thing_name,
            .authentication = {.certificate = prov->device_cert, .key = prov->device_key},
        },
        .session.keepalive = 60,
        .network.reconnect_timeout_ms = 5000,
        .buffer.size = MQTT_BUFFER_SIZE,
    };
    s_mqtt = esp_mqtt_client_init(&config);
    if (!s_mqtt || esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, on_event, NULL) != ESP_OK ||
        esp_mqtt_client_start(s_mqtt) != ESP_OK) {
        ESP_LOGE(TAG, "MQTT client failed to start: no Fleet");
        return false;
    }
    ESP_LOGI(TAG, "starting: IoT Core %s, %s", prov->iot_endpoint, s_commands);
    return true;
}

bool fleet_respond(const char *json)
{
    if (!s_mqtt || !s_connected) {
        ESP_LOGW(TAG, "not connected: response dropped: %s", json);
        return false;
    }
    return esp_mqtt_client_enqueue(s_mqtt, s_responses, json, 0, 1, 0, true) >= 0;
}
