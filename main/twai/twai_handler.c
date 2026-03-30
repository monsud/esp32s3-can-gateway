#include "twai_handler.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG           "TWAI"
#define RX_QUEUE_SIZE 64

QueueHandle_t g_twai_rx_queue = NULL;

esp_err_t twai_handler_init(void)
{
    g_twai_rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(twai_message_t));
    if (!g_twai_rx_queue) return ESP_ERR_NO_MEM;

    /* Select timing based on Kconfig baudrate */
    twai_timing_config_t t_config;
#if CONFIG_TWAI_BAUDRATE_KBPS == 250
    t_config = TWAI_TIMING_CONFIG_250KBITS();
#elif CONFIG_TWAI_BAUDRATE_KBPS == 500
    t_config = TWAI_TIMING_CONFIG_500KBITS();
#else
    t_config = TWAI_TIMING_CONFIG_1MBITS();
#endif

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        CONFIG_TWAI_TX_GPIO, CONFIG_TWAI_RX_GPIO, TWAI_MODE_NORMAL);

    /* Accept all frames — refine mask in production */
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    ESP_LOGI(TAG, "TWAI started at %d kbps (TX=%d RX=%d)",
             CONFIG_TWAI_BAUDRATE_KBPS, CONFIG_TWAI_TX_GPIO, CONFIG_TWAI_RX_GPIO);
    return ESP_OK;
}

void twai_rx_task(void *arg)
{
    twai_message_t msg;
    while (1) {
        if (twai_receive(&msg, pdMS_TO_TICKS(100)) == ESP_OK) {
            if (xQueueSend(g_twai_rx_queue, &msg, 0) != pdTRUE) {
                ESP_LOGW(TAG, "RX queue full, frame dropped (ID=0x%lX)", msg.identifier);
            }
        }
    }
}

esp_err_t twai_handler_inject(const twai_message_t *frame)
{
    esp_err_t ret = twai_transmit(frame, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Inject failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t twai_handler_get_status(twai_status_info_t *status)
{
    return twai_get_status_info(status);
}