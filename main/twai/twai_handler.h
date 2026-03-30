/**
 * @file twai_handler.h
 * @brief TWAI (CAN 2.0A/B) controller driver wrapper.
 *
 * Configures the ESP32-S3 TWAI peripheral, installs the driver,
 * and exposes a FreeRTOS queue for received frames. Supports
 * hardware acceptance filtering and remote frame injection.
 */

#pragma once
#include "driver/twai.h"
#include "freertos/queue.h"
#include "esp_err.h"

/** @brief Shared queue: twai_rx_task → mqtt_pub_task & logger_task */
extern QueueHandle_t g_twai_rx_queue;

/**
 * @brief Initialize TWAI peripheral and install driver.
 *        Creates g_twai_rx_queue and starts the driver.
 * @return ESP_OK on success
 */
esp_err_t twai_handler_init(void);

/**
 * @brief FreeRTOS task: reads frames from TWAI and posts to g_twai_rx_queue.
 * @param arg unused
 */
void twai_rx_task(void *arg);

/**
 * @brief Inject a CAN frame onto the bus.
 * @param frame Pointer to a populated twai_message_t
 * @return ESP_OK if frame was queued for transmission
 */
esp_err_t twai_handler_inject(const twai_message_t *frame);

/**
 * @brief Read current TWAI bus status counters.
 * @param[out] status Pointer to twai_status_info_t to fill
 */
esp_err_t twai_handler_get_status(twai_status_info_t *status);