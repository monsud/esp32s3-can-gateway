#include "spiffs_logger.h"
#include "twai_handler.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define TAG "LOGGER"
#define BAK_PATH CONFIG_SPIFFS_LOG_PATH ".bak"

static QueueHandle_t s_log_queue;

QueueHandle_t spiffs_logger_get_queue(void) { return s_log_queue; }

esp_err_t spiffs_logger_init(void)
{
    s_log_queue = xQueueCreate(32, sizeof(twai_message_t));

    esp_vfs_spiffs_conf_t conf = {
        .base_path       = "/spiffs",
        .partition_label = NULL,
        .max_files       = 4,
        .format_if_mount_failed = true,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPIFFS mounted");
    return ESP_OK;
}

static void rotate_if_needed(void)
{
    struct stat st;
    if (stat(CONFIG_SPIFFS_LOG_PATH, &st) == 0) {
        if (st.st_size >= CONFIG_SPIFFS_MAX_SIZE_KB * 1024) {
            remove(BAK_PATH);
            rename(CONFIG_SPIFFS_LOG_PATH, BAK_PATH);
            ESP_LOGI(TAG, "Log rotated → %s", BAK_PATH);
        }
    }
}

void logger_task(void *arg)
{
    twai_message_t msg;
    while (1) {
        if (xQueueReceive(s_log_queue, &msg, portMAX_DELAY)) {
            rotate_if_needed();
            FILE *f = fopen(CONFIG_SPIFFS_LOG_PATH, "a");
            if (f) {
                fprintf(f, "[%lu] ID=0x%03lX DLC=%d DATA=",
                        xTaskGetTickCount(), msg.identifier, msg.data_length_code);
                for (int i = 0; i < msg.data_length_code; i++)
                    fprintf(f, "%02X ", msg.data[i]);
                fprintf(f, "\n");
                fclose(f);
            }
        }
    }
}