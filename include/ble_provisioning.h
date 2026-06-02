#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "config.h"

struct ProvisioningCallbacks {
    std::function<void(const String& ssid, const String& pass)> onCredentialsReceived;
    std::function<void(const String& alias, const String& netType, float voltage)> onConfigReceived;
    std::function<void()> onClientConnected;
    std::function<void()> onClientDisconnected;
};

class BleProvisioning {
public:
    void begin(const String& deviceName, ProvisioningCallbacks cbs) {
        _cbs = cbs;

        NimBLEDevice::init(deviceName.c_str());
        NimBLEDevice::setPower(ESP_PWR_LVL_P9);

        _server = NimBLEDevice::createServer();
        _server->setCallbacks(new ServerCB(this));

        NimBLEService* svc = _server->createService(BLE_SERVICE_UUID);

        _charSsid = svc->createCharacteristic(BLE_CHAR_WIFI_SSID_UUID, NIMBLE_PROPERTY::WRITE);
        _charSsid->setCallbacks(new WriteCB(this, WriteCB::TYPE_SSID));

        _charPass = svc->createCharacteristic(BLE_CHAR_WIFI_PASS_UUID, NIMBLE_PROPERTY::WRITE);
        _charPass->setCallbacks(new WriteCB(this, WriteCB::TYPE_PASS));

        _charStatus = svc->createCharacteristic(
            BLE_CHAR_STATUS_UUID,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        _charEnergy = svc->createCharacteristic(
            BLE_CHAR_ENERGY_UUID,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        // Característica de configuração: recebe JSON { alias, netType, voltage }
        _charConfig = svc->createCharacteristic(
            BLE_CHAR_CONFIG_UUID,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );
        _charConfig->setCallbacks(new WriteCB(this, WriteCB::TYPE_CONFIG));

        svc->start();

        // UUID no advertising primário (Web Bluetooth filtra aqui)
        // Nome completo no scan response
        NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
        adv->addServiceUUID(BLE_SERVICE_UUID);
        adv->setMinPreferred(0x06);
        adv->setMaxPreferred(0x12);

        NimBLEAdvertisementData scanData;
        scanData.setName(deviceName.c_str());
        adv->setScanResponseData(scanData);

        adv->start(0);
        Serial.printf("[BLE] Advertising: %s\n", deviceName.c_str());
    }

    // Flag + valor para notify assíncrono
    // NimBLE não permite notify dentro de callbacks de escrita
    volatile bool _pendingNotify = false;
    char          _pendingStatus[32] = {};

    // Agenda um notify para ser enviado no próximo tick do loop principal
    void scheduleNotify(const char* status) {
        strncpy(_pendingStatus, status, sizeof(_pendingStatus) - 1);
        _pendingNotify = true;
        Serial.printf("[BLE] Status agendado: %s\n", status);
    }

    void scheduleNotify(const String& status) {
        scheduleNotify(status.c_str());
    }

    // Chamado pelo loop principal — envia o notify fora do contexto de callback
    void processPendingNotify() {
        if (!_pendingNotify) return;
        if (!_charStatus || _server->getConnectedCount() == 0) {
            _pendingNotify = false;
            return;
        }
        _charStatus->setValue(_pendingStatus);
        _charStatus->notify();
        Serial.printf("[BLE] Status enviado: %s\n", _pendingStatus);
        _pendingNotify = false;
    }

    // notifyStatus direto — use apenas fora de callbacks BLE
    void notifyStatus(const char* status) {
        if (_charStatus && _server->getConnectedCount() > 0) {
            _charStatus->setValue(status);
            _charStatus->notify();
            Serial.printf("[BLE] Status: %s\n", status);
        }
    }

    void notifyStatus(const String& status) {
        notifyStatus(status.c_str());
    }

    void notifyEnergy(float irms, float watts) {
        if (_charEnergy && _server->getConnectedCount() > 0) {
            char buf[48];
            snprintf(buf, sizeof(buf), "{\"irms\":%.3f,\"watts\":%.2f}", irms, watts);
            _charEnergy->setValue(buf);
            _charEnergy->notify();
        }
    }

    // Notifica o app que o device está pronto (após registrado)
    void notifyReady() {
        notifyStatus("ready");
    }

    void stop() { NimBLEDevice::getAdvertising()->stop(); }
    bool isConnected() { return _server && _server->getConnectedCount() > 0; }

private:
    NimBLEServer*         _server     = nullptr;
    NimBLECharacteristic* _charSsid   = nullptr;
    NimBLECharacteristic* _charPass   = nullptr;
    NimBLECharacteristic* _charStatus = nullptr;
    NimBLECharacteristic* _charEnergy = nullptr;
    NimBLECharacteristic* _charConfig = nullptr;

    ProvisioningCallbacks _cbs;
    String _pendingSsid;
    String _pendingPass;

    void onSsidWrite(const String& val)  { _pendingSsid = val; tryProvision(); }
    void onPassWrite(const String& val)  { _pendingPass = val; tryProvision(); }

    void tryProvision() {
        if (_pendingSsid.length() > 0 && _pendingPass.length() > 0) {
            if (_cbs.onCredentialsReceived)
                _cbs.onCredentialsReceived(_pendingSsid, _pendingPass);
            _pendingSsid = "";
            _pendingPass = "";
        }
    }

    // Recebe JSON de configuração: { "alias": "...", "netType": "mono127", "voltage": 127.0 }
    void onConfigWrite(const String& val) {
        Serial.printf("[BLE] Config recebida: %s\n", val.c_str());
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, val);
        if (err) {
            Serial.printf("[BLE] Config JSON inválido: %s\n", err.c_str());
            return;
        }
        String alias   = doc["alias"]   | "EnerTrack";
        String netType = doc["netType"] | "mono127";
        float  voltage = doc["voltage"] | 127.0f;
        if (_cbs.onConfigReceived)
            _cbs.onConfigReceived(alias, netType, voltage);
        // Usa scheduleNotify — não pode chamar notify dentro de callback BLE
        scheduleNotify("config_ok");
    }

    struct ServerCB : public NimBLEServerCallbacks {
        BleProvisioning* _p;
        ServerCB(BleProvisioning* p) : _p(p) {}
        void onConnect(NimBLEServer*) override {
            Serial.println("[BLE] Client connected");
            if (_p->_cbs.onClientConnected) _p->_cbs.onClientConnected();
        }
        void onDisconnect(NimBLEServer*) override {
            Serial.println("[BLE] Client disconnected");
            if (_p->_cbs.onClientDisconnected) _p->_cbs.onClientDisconnected();
            NimBLEDevice::getAdvertising()->start(0);
        }
    };

    struct WriteCB : public NimBLECharacteristicCallbacks {
        enum Type { TYPE_SSID, TYPE_PASS, TYPE_CONFIG };
        BleProvisioning* _p;
        Type _type;
        WriteCB(BleProvisioning* p, Type t) : _p(p), _type(t) {}
        void onWrite(NimBLECharacteristic* c) override {
            String val = c->getValue().c_str();
            switch (_type) {
                case TYPE_SSID:   _p->onSsidWrite(val);   break;
                case TYPE_PASS:   _p->onPassWrite(val);   break;
                case TYPE_CONFIG: _p->onConfigWrite(val); break;
            }
        }
    };
};