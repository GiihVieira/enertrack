#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

class NvsManager {
public:

    static bool saveWifi(
        const String& ssid,
        const String& password
    ) {
        Preferences prefs;

        if (!prefs.begin(
                NVS_NAMESPACE,
                false
            )) {
            return false;
        }

        prefs.putString(
            NVS_KEY_SSID,
            ssid
        );

        prefs.putString(
            NVS_KEY_PASS,
            password
        );

        prefs.end();

        Serial.printf(
            "[NVS] Wi-Fi saved — "
            "SSID: %s\n",
            ssid.c_str()
        );

        return true;
    }

    static bool loadWifi(
        String& ssid,
        String& password
    ) {
        Preferences prefs;

        if (!prefs.begin(
                NVS_NAMESPACE,
                true
            )) {
            return false;
        }

        ssid = prefs.getString(
            NVS_KEY_SSID,
            ""
        );

        password = prefs.getString(
            NVS_KEY_PASS,
            ""
        );

        prefs.end();

        return ssid.length() > 0;
    }

    static bool saveConfig(
        const String& alias,
        const String& netType,
        float voltage
    ) {
        Preferences prefs;

        if (!prefs.begin(
                NVS_NAMESPACE,
                false
            )) {
            return false;
        }

        prefs.putString(
            NVS_KEY_ALIAS,
            alias
        );

        prefs.putString(
            NVS_KEY_NET_TYPE,
            netType
        );

        prefs.putFloat(
            NVS_KEY_VOLTAGE,
            voltage
        );

        prefs.end();

        Serial.printf(
            "[NVS] Config saved — "
            "alias:%s netType:%s "
            "voltage:%.0fV\n",
            alias.c_str(),
            netType.c_str(),
            voltage
        );

        return true;
    }

    static String loadAlias() {
        Preferences prefs;

        prefs.begin(
            NVS_NAMESPACE,
            true
        );

        String v =
            prefs.getString(
                NVS_KEY_ALIAS,
                "EnerTrack"
            );

        prefs.end();

        return v;
    }

    static String loadNetType() {
        Preferences prefs;

        prefs.begin(
            NVS_NAMESPACE,
            true
        );

        String v =
            prefs.getString(
                NVS_KEY_NET_TYPE,
                "mono127"
            );

        prefs.end();

        return v;
    }

    static float loadVoltage() {
        Preferences prefs;

        prefs.begin(
            NVS_NAMESPACE,
            true
        );

        float v =
            prefs.getFloat(
                NVS_KEY_VOLTAGE,
                127.0f
            );

        prefs.end();

        return v;
    }

    static void setRegistered(
        bool registered
    ) {
        Preferences prefs;

        prefs.begin(
            NVS_NAMESPACE,
            false
        );

        prefs.putString(
            NVS_KEY_REGISTERED,
            registered
                ? "1"
                : "0"
        );

        prefs.end();

        Serial.printf(
            "[NVS] registered = %s\n",
            registered
                ? "true"
                : "false"
        );
    }

    static bool isRegistered() {

        static int8_t cache = -1;

        if (cache >= 0) {
            return cache == 1;
        }

        Preferences prefs;

        prefs.begin(
            NVS_NAMESPACE,
            true
        );

        bool keyExists =
            prefs.isKey(
                NVS_KEY_REGISTERED
            );

        if (!keyExists) {
            prefs.end();
            cache = 1;
            Serial.println(
                "[NVS] 'registered' "
                "não encontrado — "
                "assumindo true"
            );
            return true;
        }

        String value =
            prefs.getString(
                NVS_KEY_REGISTERED,
                "1"
            );

        prefs.end();

        bool result =
            value == "1";

        cache =
            result ? 1 : 0;

        return result;
    }

    static void clearAll() {
        Preferences prefs;

        if (prefs.begin(
                NVS_NAMESPACE,
                false
            )) {

            prefs.clear();
            prefs.end();
        }

        Serial.println(
            "[NVS] Namespace "
            "cleared — factory reset"
        );
    }

    static String getOrCreateDeviceId() {

        Preferences prefs;

        prefs.begin(
            NVS_NAMESPACE,
            false
        );

        String id =
            prefs.getString(
                NVS_KEY_DEV_ID,
                ""
            );

        if (id.isEmpty()) {

            uint8_t mac[6];

            esp_read_mac(
                mac,
                ESP_MAC_WIFI_STA
            );

            char buf[13];

            snprintf(
                buf,
                sizeof(buf),
                "%02X%02X%02X%02X%02X%02X",
                mac[0],
                mac[1],
                mac[2],
                mac[3],
                mac[4],
                mac[5]
            );

            id = String(buf);

            prefs.putString(
                NVS_KEY_DEV_ID,
                id
            );
        }

        prefs.end();

        return id;
    }
};
