#pragma once

// ─── Device ───────────────────────────────────────────────────────────────────
#define DEVICE_NAME_PREFIX   "EnerTrack"
#define FW_VERSION           "1.1.0"

// ─── Pinos ────────────────────────────────────────────────────────────────────
#define PIN_SCT013           34

// ─── SCT-013 / EmonLib ────────────────────────────────────────────────────────
#define EMON_CALIBRATION     60.6
#define EMON_SAMPLES         1480
#define MAINS_VOLTAGE        127.0   // alterado via BLE na configuração

// ─── BLE GATT UUIDs ───────────────────────────────────────────────────────────
#define BLE_SERVICE_UUID          "260068aa-c6db-4917-99ca-badb5c51f3fc"
#define BLE_CHAR_WIFI_SSID_UUID   "ac3bfe5c-27e6-4b39-8d94-2fd85a9a02d6"
#define BLE_CHAR_WIFI_PASS_UUID   "36c71103-c480-4b2c-8fe8-8ca45a22766b"
#define BLE_CHAR_STATUS_UUID      "d92bd0e1-0951-45e1-9736-50446ddb3946"
#define BLE_CHAR_ENERGY_UUID      "6d18dfb9-2356-46fd-af05-173d252b830d"
#define BLE_CHAR_CONFIG_UUID      "50a18a52-a309-4710-9600-a139e45b03ce"  // NOVO: config JSON

// ─── NVS ──────────────────────────────────────────────────────────────────────
#define NVS_NAMESPACE       "enertrack"
#define NVS_KEY_SSID        "wifi_ssid"
#define NVS_KEY_PASS        "wifi_pass"
#define NVS_KEY_DEV_ID      "device_id"
#define NVS_KEY_ALIAS       "dev_alias"     // nome do device
#define NVS_KEY_NET_TYPE    "net_type"      // "mono127" | "mono220" | "tri220" | "tri380"
#define NVS_KEY_VOLTAGE     "voltage"       // tensão em V (float)
#define NVS_KEY_REGISTERED  "registered"    // "1" quando cadastrado no backend

// ─── Wi-Fi ────────────────────────────────────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_MS  15000
#define WIFI_MAX_RETRIES         3

// ─── Timers ───────────────────────────────────────────────────────────────────
#define ENERGY_SAMPLE_INTERVAL_MS  10000UL // 10s
#define ENERGY_POST_INTERVAL_MS    60000UL // 1min

// ─── API Cloud ────────────────────────────────────────────────────────────────
#define API_READINGS_URL  "https://app.enertrack.site/api/readings"
#define API_OTA_URL       "https://app.enertrack.site/api/ota/latest"
