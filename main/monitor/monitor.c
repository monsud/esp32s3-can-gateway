#include "monitor.h"
#include "twai_handler.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define TAG "MONITOR"

void monitor_task(void *arg)
{
    uint32_t prev_rx = 0;
    TickType_t prev_tick = xTaskGetTickCount();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(CONFIG_MONITOR_PRINT_INTERVAL_MS));

        twai_status_info_t status;
        if (twai_handler_get_status(&status) != ESP_OK) continue;

        TickType_t now  = xTaskGetTickCount();
        uint32_t   dt   = (now - prev_tick) / configTICK_RATE_HZ;  /* seconds */
        uint32_t   fps  = dt ? (status.msgs_to_rx - prev_rx) / dt : 0;

        ESP_LOGI(TAG, "── CAN Monitor ──────────────────");
        ESP_LOGI(TAG, "  State     : %d", status.state);
        ESP_LOGI(TAG, "  RX frames : %lu  (%lu fps)", status.msgs_to_rx, fps);
        ESP_LOGI(TAG, "  TX frames : %lu", status.msgs_to_tx);
        ESP_LOGI(TAG, "  RX errors : %lu", status.rx_error_counter);
        ESP_LOGI(TAG, "  TX errors : %lu", status.tx_error_counter);
        ESP_LOGI(TAG, "─────────────────────────────────");

        /* Alarm on bus-off or error-passive */
        if (status.state == TWAI_STATE_BUS_OFF) {
            ESP_LOGE(TAG, "BUS-OFF detected!");
            mqtt_publish_alarm("{\"alarm\":\"BUS_OFF\"}");
            twai_initiate_recovery();
        } else if (status.rx_error_counter > 127 || status.tx_error_counter > 127) {
            mqtt_publish_alarm("{\"alarm\":\"ERROR_PASSIVE\"}");
        }

        prev_rx   = status.msgs_to_rx;
        prev_tick = now;
    }
}