/**
 * @file main.c
 * @brief Entry point — initializes all subsystems and spawns FreeRTOS tasks.
 *
 * Boot sequence:
 *  1. WiFi connect
 *  2. SPIFFS mount
 *  3. TWAI driver start
 *  4. MQTT broker connect
 *  5. Spawn tasks: twai_rx, mqtt_pub, mqtt_sub, logger, monitor
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include "twai_handler.h"
#include "mqtt_client.h"
#include "spiffs_logger.h"
#include "monitor.h"

#define TAG "MAIN"

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-S3 CAN Gateway booting ===");

    /* 1. WiFi */
    ESP_ERROR_CHECK(wifi_manager_init());

    /* 2. SPIFFS */
    ESP_ERROR_CHECK(spiffs_logger_init());

    /* 3. TWAI */
    ESP_ERROR_CHECK(twai_handler_init());

    /* 4. MQTT */
    ESP_ERROR_CHECK(mqtt_manager_init());

    /* 5. Tasks */
    xTaskCreatePinnedToCore(twai_rx_task,  "twai_rx",  4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(mqtt_pub_task, "mqtt_pub", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(mqtt_sub_task, "mqtt_sub", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(logger_task,   "logger",   4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(monitor_task,  "monitor",  4096, NULL, 2, NULL, 0);

    ESP_LOGI(TAG, "All tasks started.");
}