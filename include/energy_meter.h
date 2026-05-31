#pragma once

#include <Arduino.h>
#include <EmonLib.h>

#include "config.h"

#define ENERGY_MIN_VALID_IRMS 0.20f
#define ENERGY_MAX_VALID_IRMS 35.00f

class EnergyMeter {
public:
    void begin() {
        pinMode(PIN_SCT013, INPUT);
        analogReadResolution(12);

        _emon.current(PIN_SCT013, EMON_CALIBRATION);

        Serial.printf(
            "[ENERGY] SCT-013 initialized — pin %d, calibration %.1f\n",
            PIN_SCT013,
            EMON_CALIBRATION
        );
    }

    bool read(float voltage) {
        float irms = _emon.calcIrms(EMON_SAMPLES);

        if (!isfinite(irms)) {
            discard("non-finite");
            return false;
        }

        if (irms < ENERGY_MIN_VALID_IRMS) {
            _irms = 0.0f;
            _watts = 0.0f;
            Serial.println("[ENERGY] No load detected");
            return true;
        }

        if (irms > ENERGY_MAX_VALID_IRMS) {
            discard("out-of-range current");
            return false;
        }

        _irms = irms;
        _watts = _irms * voltage;

        Serial.printf(
            "[ENERGY] Irms: %.3f A | Voltage: %.0f V | Power: %.2f W\n",
            _irms,
            voltage,
            _watts
        );

        return true;
    }

    float getIrms() const {
        return _irms;
    }

    float getWatts() const {
        return _watts;
    }

private:
    EnergyMonitor _emon;

    float _irms = 0.0f;
    float _watts = 0.0f;

    void discard(const char* reason) {
        _irms = 0.0f;
        _watts = 0.0f;

        Serial.printf(
            "[ENERGY] Invalid reading discarded — reason: %s\n",
            reason
        );
    }
};