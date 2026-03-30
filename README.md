# ESP32-S3 CAN Gateway

![ESP32](https://img.shields.io/badge/ESP32--S3-ESP--IDF%20v5.5.3-blue)
![FreeRTOS](https://img.shields.io/badge/FreeRTOS-multicore-green)
![MQTT](https://img.shields.io/badge/MQTT-HiveMQ-orange)
![CAN](https://img.shields.io/badge/TWAI-CAN%202.0A%2FB-red)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

## Overview

A production-grade automotive CAN Bus Gateway firmware for the ESP32-S3.
The node receives CAN 2.0A/B frames via the TWAI peripheral, forwards
them as structured JSON telemetry to an MQTT broker (HiveMQ), and accepts
remote frame injection commands over MQTT. All frames are persisted to
flash (SPIFFS) with automatic log rotation. A real-time UART monitor
tracks bus health and publishes alarms on fault conditions.

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    APPLICATION LAYER                     │
│  twai_rx_task → mqtt_pub_task   mqtt_sub_task           │
│       ↓               ↓               ↓                 │
│  logger_task      monitor_task                          │
└───────────────────────────┬──────────────────────────────┘
                            │  FreeRTOS Queues
┌───────────────────────────▼──────────────────────────────┐
│                   MIDDLEWARE / RTOS LAYER                │
│   g_twai_rx_queue   s_inject_queue   s_log_queue        │
│   s_connected_sem  (MQTT ready gate)                    │
└───────────────────────────┬──────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────┐
│                     DRIVERS LAYER                        │
│   twai_handler   (ESP32-S3 TWAI peripheral)             │
│   wifi_manager   (esp_wifi + auto-reconnect)            │
│   mqtt_client    (esp-mqtt + cJSON serialisation)       │
│   spiffs_logger  (SPIFFS + rotation)                    │
└──────────────────────────────────────────────────────────┘
```

### Data Flow

```
CAN Bus ──► twai_rx_task ──[g_twai_rx_queue]──► mqtt_pub_task ──► HiveMQ
                    │
                    └──[s_log_queue]──► logger_task ──► SPIFFS

HiveMQ ──► mqtt_sub_task ──[s_inject_queue]──► twai_handler_inject ──► CAN Bus
```

---

## Project Structure

```
esp32s3-can-gateway/
├── README.md
├── CMakeLists.txt
├── partitions.csv
├── sdkconfig.defaults
├── Kconfig.projbuild
│
└── main/
    ├── CMakeLists.txt
    ├── main.c
    ├── wifi/
    │   ├── wifi_manager.h
    │   └── wifi_manager.c
    ├── twai/
    │   ├── twai_handler.h
    │   └── twai_handler.c
    ├── mqtt/
    │   ├── mqtt_client.h
    │   └── mqtt_client.c
    ├── logger/
    │   ├── spiffs_logger.h
    │   └── spiffs_logger.c
    └── monitor/
        ├── monitor.h
        └── monitor.c
```

---

## RTOS Task Map

| Task           | Priority | Stack | Core | Period       | Purpose                        |
|----------------|----------|-------|------|--------------|--------------------------------|
| twai_rx_task   | 6        | 4 KB  | 1    | event-driven | TWAI frame reception           |
| mqtt_pub_task  | 5        | 4 KB  | 0    | event-driven | JSON publish to HiveMQ         |
| mqtt_sub_task  | 5        | 4 KB  | 0    | event-driven | Inject command reception       |
| logger_task    | 3        | 4 KB  | 0    | event-driven | SPIFFS frame logging           |
| monitor_task   | 2        | 4 KB  | 0    | 5 s          | Bus health + UART stats        |

`twai_rx_task` is pinned to **Core 1** to isolate real-time CAN reception
from network I/O tasks on Core 0.

---

## Inter-Task Communication

```
  twai_rx_task ─[twai_message_t]──► g_twai_rx_queue ──► mqtt_pub_task
  twai_rx_task ─[twai_message_t]──► s_log_queue     ──► logger_task
  mqtt_sub_task ─[twai_message_t]─► s_inject_queue  ──► twai_handler_inject
  monitor_task ──────────────────────────────────────► mqtt_publish_alarm
```

---

## MQTT Topics

| Topic                  | Direction     | Payload                              |
|------------------------|---------------|--------------------------------------|
| `cangateway/telemetry` | ESP32 → Cloud | `{"id":291,"dlc":8,"ts":1234,"data":[1,2,...]}` |
| `cangateway/inject`    | Cloud → ESP32 | `{"id":291,"dlc":2,"data":[1,2]}`   |
| `cangateway/alarm`     | ESP32 → Cloud | `{"alarm":"BUS_OFF"}`               |

---

## Hardware Wiring

```
ESP32-S3          SN65HVD230 / TJA1050
─────────         ──────────────────────
GPIO5  (TX)  ──►  TXD
GPIO6  (RX)  ◄──  RXD
3.3V         ──►  VCC
GND          ──►  GND
             ──►  CANH ──┐
                          ├── CAN Bus
             ──►  CANL ──┘
```

> For bench testing without a live bus set `TWAI_MODE_NO_ACK` in
> `twai_handler.c`.

---

## Configuration (Kconfig / menuconfig)

| Symbol                      | Default                          | Description               |
|-----------------------------|----------------------------------|---------------------------|
| `CONFIG_WIFI_SSID`          | `"MyNetwork"`                    | WiFi SSID                 |
| `CONFIG_WIFI_PASSWORD`      | `"MyPassword"`                   | WiFi password             |
| `CONFIG_MQTT_BROKER_URI`    | `"mqtt://broker.hivemq.com:1883"`| Broker URI                |
| `CONFIG_MQTT_CLIENT_ID`     | `"esp32s3-can-gateway"`          | MQTT client identifier    |
| `CONFIG_TWAI_TX_GPIO`       | `5`                              | CAN TX pin                |
| `CONFIG_TWAI_RX_GPIO`       | `6`                              | CAN RX pin                |
| `CONFIG_TWAI_BAUDRATE_KBPS` | `500`                            | 250 / 500 / 1000 kbps     |
| `CONFIG_SPIFFS_LOG_PATH`    | `"/spiffs/can_log.txt"`          | Log file path             |
| `CONFIG_SPIFFS_MAX_SIZE_KB` | `64`                             | Rotation threshold        |
| `CONFIG_MONITOR_PRINT_INTERVAL_MS` | `5000`                  | Stats print period        |

---

## Build & Flash

```bash
# Prerequisites: ESP-IDF >= 5.5 sourced
. $IDF_PATH/export.sh

# Configure credentials and parameters
idf.py menuconfig
# → CAN Gateway Configuration

# Build
idf.py build

# Flash + monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Error Handling Strategy

| Condition              | Response                                        |
|------------------------|-------------------------------------------------|
| TWAI RX queue full     | Drop frame, log warning                        |
| WiFi disconnect        | Auto-reconnect (max 10 retries), log event     |
| MQTT disconnect        | esp-mqtt auto-reconnects transparently         |
| CAN Bus-Off state      | Publish alarm, call `twai_initiate_recovery()` |
| Error Passive (EC>127) | Publish alarm to `cangateway/alarm`            |
| SPIFFS full            | Log rotation → rename to `.bak`               |
| Inject parse failure   | cJSON error, discard payload, log warning      |

---

## Log Format (SPIFFS `/spiffs/can_log.txt`)

```
 ID=0x1A2 DLC=8 DATA=01 02 03 04 05 06 07 08
 ID=0x0CF DLC=4 DATA=FF 00 A3 12
 ID=0x2B0 DLC=2 DATA=DE AD
```

---

## Requirements Traceability

| Requirement                        | Implementation                              |
|------------------------------------|---------------------------------------------|
| RF-01 CAN 2.0A/B reception         | `twai_handler.c` + TWAI peripheral          |
| RF-02 Frame injection              | `twai_handler_inject()` + `mqtt_sub_task`   |
| RF-03 Hardware acceptance filter   | `twai_filter_config_t` in `twai_handler.c`  |
| RF-04 MQTT forwarding (JSON)       | `mqtt_pub_task` + cJSON                     |
| RF-05 SPIFFS logging + rotation    | `spiffs_logger.c`                           |
| RF-06 Real-time bus statistics     | `monitor_task` + `twai_get_status_info()`   |
| RF-07 Runtime configuration        | `Kconfig.projbuild` + `idf.py menuconfig`   |
| RF-08 Error detection & alarms     | `monitor_task` + `mqtt_publish_alarm()`     |
| RF-09 WiFi auto-reconnect          | `wifi_manager.c` event handler              |
| RF-10 Multi-task RTOS              | FreeRTOS, 5 concurrent tasks, dual-core     |

---

## License

MIT — see [LICENSE](LICENSE)
