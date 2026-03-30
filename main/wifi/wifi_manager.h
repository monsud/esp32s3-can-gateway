/**
 * @file wifi_manager.h
 * @brief WiFi manager with auto-reconnect logic.
 *
 * Initializes the ESP32-S3 WiFi station, connects to the configured
 * SSID and handles reconnection transparently via event loop callbacks.
 */

#pragma once
#include "esp_err.h"

/**
 * @brief Initialize and start WiFi in STA mode.
 *        Blocks until IP is obtained or timeout.
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Returns true if WiFi is currently connected.
 */
bool wifi_manager_is_connected(void);