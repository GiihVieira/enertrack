#pragma once

// ─── Device ───────────────────────────────────────────────────────────────────
#define DEVICE_NAME_PREFIX   "EnerTrack"
#define FW_VERSION           "1.0.1"

// ─── Pinos ────────────────────────────────────────────────────────────────────
#define PIN_SCT013           34

// ─── SCT-013 / EmonLib ────────────────────────────────────────────────────────
#define EMON_CALIBRATION     60.6
#define EMON_SAMPLES         1480
#define MAINS_VOLTAGE        127.0   // alterado via BLE na configuração

// ─── BLE GATT UUIDs ───────────────────────────────────────────────────────────
#define BLE_SERVICE_UUID          "12345678-1234-1234-1234-123456789abc"
#define BLE_CHAR_WIFI_SSID_UUID   "12345678-1234-1234-1234-123456789ab1"
#define BLE_CHAR_WIFI_PASS_UUID   "12345678-1234-1234-1234-123456789ab2"
#define BLE_CHAR_STATUS_UUID      "12345678-1234-1234-1234-123456789ab3"
#define BLE_CHAR_ENERGY_UUID      "12345678-1234-1234-1234-123456789ab4"
#define BLE_CHAR_CONFIG_UUID      "12345678-1234-1234-1234-123456789ab6"  // NOVO: config JSON

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
#define ENERGY_POST_INTERVAL_MS    30000UL // 5min

// ─── API Cloud ────────────────────────────────────────────────────────────────
#define API_READINGS_URL  "https://enertrack-web.giihvieiratwo.workers.dev/api/readings"
#define API_OTA_URL       "https://enertrack-web.giihvieiratwo.workers.dev/api/ota/latest"
