/**
 * @file mqtt_client.h
 * @brief MQTT client wrapper over esp-mqtt.
 *
 * Connects to HiveMQ broker, publishes CAN telemetry as JSON,
 * subscribes to the inject topic and handles reconnection.
 */

#pragma once
#include "driver/twai.h"
#include "esp_err.h"

/**
 * @brief Initialize and connect MQTT client.
 * @return ESP_OK when broker connection is established
 */
esp_err_t mqtt_manager_init(void);

/**
 * @brief FreeRTOS task: consumes g_twai_rx_queue and publishes to broker.
 * @param arg unused
 */
void mqtt_pub_task(void *arg);

/**
 * @brief FreeRTOS task: listens on inject topic and calls twai_handler_inject.
 * @param arg unused
 */
void mqtt_sub_task(void *arg);

/**
 * @brief Publish a plain alarm string to CONFIG_MQTT_TOPIC_ALARM.
 * @param msg Null-terminated alarm message
 */
void mqtt_publish_alarm(const char *msg);