/**
 * @file monitor.h
 * @brief Real-time UART statistics monitor.
 *
 * Periodically prints TWAI bus statistics (frame rate, TX/RX counters,
 * bus state) to the serial console. Triggers an MQTT alarm on
 * bus-off or error-passive conditions.
 */

#pragma once

/**
 * @brief FreeRTOS task: prints stats every CONFIG_MONITOR_PRINT_INTERVAL_MS ms.
 * @param arg unused
 */
void monitor_task(void *arg);