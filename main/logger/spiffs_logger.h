/**
 * @file spiffs_logger.h
 * @brief Persistent SPIFFS logger with automatic file rotation.
 *
 * Writes CAN frames to a flat text file on SPIFFS. When the file
 * exceeds CONFIG_SPIFFS_MAX_SIZE_KB the file is rotated (renamed
 * to .bak) and a new log is started.
 */

#pragma once
#include "driver/twai.h"
#include "esp_err.h"

/**
 * @brief Mount SPIFFS partition and open log file.
 * @return ESP_OK on success
 */
esp_err_t spiffs_logger_init(void);

/**
 * @brief FreeRTOS task: consumes g_twai_rx_queue copy and writes to SPIFFS.
 * @param arg unused
 */
void logger_task(void *arg);