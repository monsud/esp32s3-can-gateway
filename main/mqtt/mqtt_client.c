#include "mqtt_client.h"
#include "twai_handler.h"
#include "esp_log.h"
#include "esp_mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "sdkconfig.h"
#include <string.h>
#include <stdio.h>

#define TAG "MQTT"

static esp_mqtt_client_handle_t s_client = NULL;
static SemaphoreHandle_t        s_connected_sem;

/* Inject queue: mqtt_sub_task → twai_rx_task direction */
static QueueHandle_t s_inject_queue;

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to broker");
            esp_mqtt_client_subscribe(s_client, CONFIG_MQTT_TOPIC_INJECT, 1);
            xSemaphoreGive(s_connected_sem);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected — will auto-reconnect");
            break;
        case MQTT_EVENT_DATA: {
            /* Parse inject command: {"id":123,"dlc":2,"data":[1,2]} */
            char *payload = strndup(event->data, event->data_len);
            cJSON *root = cJSON_Parse(payload);
            if (root) {
                twai_message_t frame = {0};
                frame.identifier = (uint32_t)cJSON_GetObjectItem(root, "id")->valueint;
                frame.data_length_code = (uint8_t)cJSON_GetObjectItem(root, "dlc")->valueint;
                cJSON *arr = cJSON_GetObjectItem(root, "data");
                for (int i = 0; i < frame.data_length_code && i < 8; i++)
                    frame.data[i] = (uint8_t)cJSON_GetArrayItem(arr, i)->valueint;
                xQueueSend(s_inject_queue, &frame, 0);
                cJSON_Delete(root);
            }
            free(payload);
            break;
        }
        default: break;
    }
}

esp_err_t mqtt_manager_init(void)
{
    s_connected_sem = xSemaphoreCreateBinary();
    s_inject_queue  = xQueueCreate(8, sizeof(twai_message_t));

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .credentials.client_id = CONFIG_MQTT_CLIENT_ID,
    };
    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    /* Block until connected (max 15s) */
    if (!xSemaphoreTake(s_connected_sem, pdMS_TO_TICKS(15000))) {
        ESP_LOGE(TAG, "Broker connection timeout");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void mqtt_pub_task(void *arg)
{
    twai_message_t msg;
    char buf[256];
    while (1) {
        if (xQueueReceive(g_twai_rx_queue, &msg, portMAX_DELAY)) {
            /* Build JSON payload */
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "id",  (double)msg.identifier);
            cJSON_AddNumberToObject(root, "dlc", msg.data_length_code);
            cJSON_AddNumberToObject(root, "ts",  (double)xTaskGetTickCount());
            cJSON *arr = cJSON_AddArrayToObject(root, "data");
            for (int i = 0; i < msg.data_length_code; i++)
                cJSON_AddItemToArray(arr, cJSON_CreateNumber(msg.data[i]));

            cJSON_PrintPreallocated(root, buf, sizeof(buf), false);
            esp_mqtt_client_publish(s_client, CONFIG_MQTT_TOPIC_TELEMETRY,
                                    buf, 0, 1, 0);
            cJSON_Delete(root);
        }
    }
}

void mqtt_sub_task(void *arg)
{
    twai_message_t frame;
    while (1) {
        if (xQueueReceive(s_inject_queue, &frame, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Injecting frame ID=0x%lX", frame.identifier);
            twai_handler_inject(&frame);
        }
    }
}

void mqtt_publish_alarm(const char *msg)
{
    if (s_client)
        esp_mqtt_client_publish(s_client, CONFIG_MQTT_TOPIC_ALARM,
                                msg, 0, 1, 0);
}