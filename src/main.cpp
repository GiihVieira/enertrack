#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

#include "config.h"
#include "device_state.h"
#include "nvs_manager.h"
#include "ble_provisioning.h"
#include "energy_meter.h"

// ─── Factory Reset ────────────────────────────────────────────────────────────
#define PIN_FACTORY_RESET    0      // GPIO0 = botão BOOT do ESP32 DevKit
#define RESET_HOLD_MS        5000   // segurar 5s para resetar

// ─── Instâncias globais ───────────────────────────────────────────────────────
static DeviceState     gState  = DeviceState::BOOT;
static BleProvisioning gBle;
static EnergyMeter     gEnergy;

static volatile bool   gGotCredentials = false;
static volatile bool   gGotConfig      = false;
static String          gPendingSsid;
static String          gPendingPass;
static String          gPendingAlias;
static String          gPendingNetType;
static float           gPendingVoltage = 127.0f;

// ─── Helpers ──────────────────────────────────────────────────────────────────
void setState(DeviceState next) {
    Serial.printf("[STATE] %s → %s\n", stateToStr(gState), stateToStr(next));
    gState = next;
}

String buildDeviceName() {
    String id = NvsManager::getOrCreateDeviceId();
    return String(DEVICE_NAME_PREFIX) + "-" + id;
}

bool connectWifi(const String& ssid, const String& pass) {
    Serial.printf("[WIFI] Conectando a: %s\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            WiFi.disconnect(true);
            return false;
        }
        delay(300);
    }
    Serial.printf("[WIFI] Conectado! IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

void postReading(float irms, float watts) {
    if (WiFi.status() != WL_CONNECTED) return;
    if (strlen(API_READINGS_URL) == 0)  return;

    HTTPClient http;
    http.begin(API_READINGS_URL);
    http.addHeader("Content-Type", "application/json");

    String mac  = NvsManager::getOrCreateDeviceId();
    String body = "{\"mac_address\":\"" + mac + "\","
                  "\"irms\":"  + String(irms, 3) + ","
                  "\"watts\":" + String(watts, 2) + "}";

    int code = http.POST(body);
    http.end();
    Serial.printf("[HTTP] POST readings → %d\n", code);
}

// Verifica se o device está registrado no backend
// Chama o endpoint /api/devices/register para confirmar
void checkRegistration() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (NvsManager::isRegistered())    return;  // Já confirmado

    String url = String(API_READINGS_URL);
    // Monta URL do endpoint de registro a partir da URL de readings
    url = url.substring(0, url.lastIndexOf('/') + 1) + "devices/register";

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String mac  = NvsManager::getOrCreateDeviceId();
    String body = "{\"mac_address\":\"" + mac + "\"," 
                  "\"fw_version\":\"" + String(FW_VERSION) + "\"}";

    int code = http.POST(body);
    String resp = http.getString();
    http.end();

    if (code == 200 && resp.indexOf("\"registered\":true") >= 0) {
        NvsManager::setRegistered(true);
        Serial.println("[REG] Device confirmado no backend ✓");
    } else {
        Serial.printf("[REG] Não registrado ainda (%d) — tentará novamente\n", code);
    }
}

void checkOta() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (strlen(API_OTA_URL) == 0)      return;

    HTTPClient http;
    http.begin(API_OTA_URL);
    http.addHeader("X-Device-Version", FW_VERSION);
    http.addHeader("X-Device-Mac", NvsManager::getOrCreateDeviceId());
    int code = http.GET();
    if (code != 200) { http.end(); return; }

    String payload = http.getString();
    http.end();

    if (payload.indexOf("\"update\":true") < 0) {
        Serial.println("[OTA] Firmware atualizado");
        return;
    }

    int urlStart = payload.indexOf("\"url\":\"") + 7;
    int urlEnd   = payload.indexOf("\"", urlStart);
    if (urlStart < 7 || urlEnd < 0) return;

    String binUrl = payload.substring(urlStart, urlEnd);
    Serial.printf("[OTA] Baixando: %s\n", binUrl.c_str());

    HTTPClient httpBin;
    httpBin.begin(binUrl);
    httpBin.setTimeout(60000);
    int binCode = httpBin.GET();
    if (binCode != 200) { httpBin.end(); return; }

    int contentLen = httpBin.getSize();
    WiFiClient* stream = httpBin.getStreamPtr();

    if (!Update.begin(contentLen)) { httpBin.end(); return; }
    size_t written = Update.writeStream(*stream);
    httpBin.end();

    if (written == (size_t)contentLen && Update.end(true)) {
        Serial.println("[OTA] Atualização concluída! Reiniciando...");
        delay(500);
        ESP.restart();
    }
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.printf("\n=== EnerTrack Home v%s ===\n", FW_VERSION);
    Serial.printf("[DEVICE] MAC: %s\n", NvsManager::getOrCreateDeviceId().c_str());

    // Configura botão de factory reset (GPIO0 = BOOT)
    pinMode(PIN_FACTORY_RESET, INPUT_PULLUP);

    String ssid, pass;
    if (NvsManager::loadWifi(ssid, pass)) {
        setState(DeviceState::WIFI_CONNECTING);
        if (connectWifi(ssid, pass)) {
            gEnergy.begin();
            checkRegistration();  // Confirma registro no backend
            checkOta();
            setState(DeviceState::ONLINE);
            return;
        }
        NvsManager::clearAll();
    }

    gEnergy.begin();

    ProvisioningCallbacks cbs;

    cbs.onClientConnected    = []() { setState(DeviceState::BLE_CONNECTED); };
    cbs.onClientDisconnected = []() {
        if (gState == DeviceState::BLE_CONNECTED)
            setState(DeviceState::BLE_ADVERTISING);
    };

    // Credenciais Wi-Fi recebidas
    cbs.onCredentialsReceived = [](const String& ssid, const String& pass) {
        gPendingSsid    = ssid;
        gPendingPass    = pass;
        gGotCredentials = true;
    };

    // Configuração recebida (alias, tipo de rede, tensão)
    cbs.onConfigReceived = [](const String& alias, const String& netType, float voltage) {
        gPendingAlias   = alias;
        gPendingNetType = netType;
        gPendingVoltage = voltage;
        gGotConfig      = true;
    };

    gBle.begin(buildDeviceName(), cbs);
    setState(DeviceState::BLE_ADVERTISING);
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
static unsigned long lastEnergyRead = 0;
static unsigned long lastOtaCheck   = 0;
const  unsigned long OTA_INTERVAL   = 3600000UL;

// Verifica botão de factory reset — segurar GPIO0 por 5s
void checkFactoryReset() {
    if (digitalRead(PIN_FACTORY_RESET) != LOW) return;

    unsigned long pressStart = millis();
    Serial.println("[RESET] Segure 5s para factory reset...");

    while (digitalRead(PIN_FACTORY_RESET) == LOW) {
        unsigned long held = millis() - pressStart;
        if (held % 1000 < 50) Serial.printf("[RESET] %lus...\n", held / 1000 + 1);
        if (held >= RESET_HOLD_MS) {
            Serial.println("[RESET] Factory reset!");
            NvsManager::clearAll();
            delay(300);
            ESP.restart();
        }
        delay(50);
    }
    Serial.println("[RESET] Cancelado");
}

void loop() {
    checkFactoryReset();

    // Processa notificações BLE pendentes (agendadas dentro de callbacks)
    gBle.processPendingNotify();

    switch (gState) {

        case DeviceState::BLE_ADVERTISING:
        case DeviceState::BLE_CONNECTED:

            // Configuração recebida — salva no NVS e notifica via scheduleNotify
            if (gGotConfig) {
                gGotConfig = false;
                NvsManager::saveConfig(gPendingAlias, gPendingNetType, gPendingVoltage);
                // config_ok já foi agendado via scheduleNotify no onConfigWrite
                Serial.printf("[CONFIG] alias=%s netType=%s voltage=%.0fV\n",
                    gPendingAlias.c_str(), gPendingNetType.c_str(), gPendingVoltage);
            }

            // Credenciais Wi-Fi recebidas — conecta e reinicia
            if (gGotCredentials) {
                gGotCredentials = false;
                setState(DeviceState::WIFI_CONNECTING);
                gBle.notifyStatus("connecting");

                if (connectWifi(gPendingSsid, gPendingPass)) {
                    NvsManager::saveWifi(gPendingSsid, gPendingPass);

                    // Agenda notify e aguarda loop processar antes de parar BLE
                    gBle.scheduleNotify("wifi_ok");
                    for (int i = 0; i < 30; i++) {
                        gBle.processPendingNotify();
                        delay(100);
                    }

                    gBle.stop();
                    delay(300);
                    ESP.restart();

                } else {
                    gBle.scheduleNotify("wifi_fail");
                    for (int i = 0; i < 10; i++) {
                        gBle.processPendingNotify();
                        delay(100);
                    }
                    setState(DeviceState::BLE_CONNECTED);
                }
            }
            break;

        case DeviceState::ONLINE:
            // Leituras periódicas
            if (millis() - lastEnergyRead >= ENERGY_POST_INTERVAL_MS) {
                lastEnergyRead = millis();
                float voltage = NvsManager::loadVoltage();

                if (gEnergy.read(voltage)) {
                    postReading(gEnergy.getIrms(), gEnergy.getWatts());
                }
            }

            // OTA periódico
            if (millis() - lastOtaCheck >= OTA_INTERVAL) {
                lastOtaCheck = millis();
                checkOta();
            }

            // Reconexão Wi-Fi
            if (WiFi.status() != WL_CONNECTED) {
                setState(DeviceState::WIFI_CONNECTING);
                String ssid, pass;
                if (NvsManager::loadWifi(ssid, pass) && connectWifi(ssid, pass))
                    setState(DeviceState::ONLINE);
                else { NvsManager::clearAll(); ESP.restart(); }
            }
            break;

        case DeviceState::WIFI_CONNECTING:
            delay(100);
            break;

        default: break;
    }
}