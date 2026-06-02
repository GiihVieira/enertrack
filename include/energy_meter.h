#pragma once

#include <Arduino.h>
#include <EmonLib.h>

#include "config.h"

#define ENERGY_MIN_VALID_IRMS 0.20f
#define ENERGY_MAX_VALID_IRMS 100.00f
#define ENERGY_L1_CORRECTION_FACTOR 0.2041f
#define ENERGY_L2_CORRECTION_FACTOR 0.1089f

#define ENERGY_WARMUP_READS 5
#define ENERGY_WARMUP_DELAY_MS 200

#ifndef ENERGY_BUFFER_SIZE
#define ENERGY_BUFFER_SIZE 6
#endif

#ifndef PIN_SCT013_L1
#define PIN_SCT013_L1 34
#endif

#ifndef PIN_SCT013_L2
#define PIN_SCT013_L2 35
#endif

struct EnergySample {
    float l1;
    float l2;
    float total;
    float watts;
};

class EnergyMeter {
public:
    void begin() {
        pinMode(PIN_SCT013_L1, INPUT);
        pinMode(PIN_SCT013_L2, INPUT);

        analogReadResolution(12);

        _emonL1.current(PIN_SCT013_L1, EMON_CALIBRATION);
        _emonL2.current(PIN_SCT013_L2, EMON_CALIBRATION);

        clearBuffer();

        Serial.printf(
            "[ENERGY] SCT-013 initialized — L1 pin %d, L2 pin %d, calibration %.1f\n",
            PIN_SCT013_L1,
            PIN_SCT013_L2,
            EMON_CALIBRATION
        );

        warmup();
    }

    bool read(float voltage) {
        float measuredL1 = _emonL1.calcIrms(EMON_SAMPLES);
        float measuredL2 = _emonL2.calcIrms(EMON_SAMPLES);

        float rawL1 = measuredL1 * ENERGY_L1_CORRECTION_FACTOR;
        float rawL2 = measuredL2 * ENERGY_L2_CORRECTION_FACTOR;

        if (!_firstRuntimeReadingDiscarded) {
            _firstRuntimeReadingDiscarded = true;
            discard("first runtime reading warm-up");
            return false;
        }

        Serial.println("[ENERGY][DEBUG] ------------------------------");

        Serial.printf(
            "[ENERGY][DEBUG] MEASURED L1: %.6f A | MEASURED L2: %.6f A\n",
            measuredL1,
            measuredL2
        );

        Serial.printf(
            "[ENERGY][DEBUG] CORRECTED L1: %.6f A | CORRECTED L2: %.6f A\n",
            rawL1,
            rawL2
        );

        if (!isValidIrms(rawL1) || !isValidIrms(rawL2)) {
            discard("non-finite");
            return false;
        }

        float irmsL1 = normalizeLowCurrent(rawL1);
        float irmsL2 = normalizeLowCurrent(rawL2);

        Serial.printf(
            "[ENERGY][DEBUG] NORMALIZED L1: %.6f A | NORMALIZED L2: %.6f A\n",
            irmsL1,
            irmsL2
        );

        if (irmsL1 > ENERGY_MAX_VALID_IRMS || irmsL2 > ENERGY_MAX_VALID_IRMS) {
            Serial.printf(
                "[ENERGY][DEBUG] OUT-OF-RANGE | L1: %.6f A | L2: %.6f A | Max: %.2f A\n",
                irmsL1,
                irmsL2,
                ENERGY_MAX_VALID_IRMS
            );

            discard("out-of-range current");
            return false;
        }

        EnergySample sample;
        sample.l1 = irmsL1;
        sample.l2 = irmsL2;
        sample.total = irmsL1 + irmsL2;
        sample.watts = sample.total * voltage;

        addSample(sample);
        updateAggregatedReading();

        Serial.printf(
            "[ENERGY][SAMPLE] L1: %.3f A | L2: %.3f A | Total: %.3f A | Power: %.2f W | Buffer: %d/%d\n",
            sample.l1,
            sample.l2,
            sample.total,
            sample.watts,
            _sampleCount,
            ENERGY_BUFFER_SIZE
        );

        Serial.printf(
            "[ENERGY][WINDOW] L1: %.3f A | L2: %.3f A | Total: %.3f A | Voltage: %.0f V | Power: %.2f W\n",
            _irmsL1,
            _irmsL2,
            _irms,
            voltage,
            _watts
        );

        if (_irms <= 0.0f) {
            Serial.println("[ENERGY] No load detected");
            return true;
        }

        return true;
    }

    bool hasSamples() const {
        return _sampleCount > 0;
    }

    void clearSamples() {
        clearBuffer();
    }

    float getIrms() const {
        return _irms;
    }

    float getIrmsL1() const {
        return _irmsL1;
    }

    float getIrmsL2() const {
        return _irmsL2;
    }

    float getWatts() const {
        return _watts;
    }

private:
    EnergyMonitor _emonL1;
    EnergyMonitor _emonL2;

    EnergySample _samples[ENERGY_BUFFER_SIZE];

    int _sampleCount = 0;
    int _sampleIndex = 0;

    float _irms = 0.0f;
    float _irmsL1 = 0.0f;
    float _irmsL2 = 0.0f;
    float _watts = 0.0f;

    bool _firstRuntimeReadingDiscarded = false;

    void warmup() {
        Serial.println("[ENERGY] Warm-up started");

        for (int i = 0; i < ENERGY_WARMUP_READS; i++) {
            _emonL1.calcIrms(EMON_SAMPLES);
            _emonL2.calcIrms(EMON_SAMPLES);
            delay(ENERGY_WARMUP_DELAY_MS);
        }

        resetValues();
        clearBuffer();

        Serial.println("[ENERGY] Warm-up completed");
    }

    bool isValidIrms(float value) const {
        return isfinite(value) && value >= 0.0f;
    }

    float normalizeLowCurrent(float value) const {
        if (value < ENERGY_MIN_VALID_IRMS) {
            return 0.0f;
        }

        return value;
    }

    void addSample(const EnergySample& sample) {
        _samples[_sampleIndex] = sample;
        _sampleIndex = (_sampleIndex + 1) % ENERGY_BUFFER_SIZE;

        if (_sampleCount < ENERGY_BUFFER_SIZE) {
            _sampleCount++;
        }
    }

    float median(float values[], int count) const {
        for (int i = 0; i < count - 1; i++) {
            for (int j = i + 1; j < count; j++) {
                if (values[j] < values[i]) {
                    float tmp = values[i];
                    values[i] = values[j];
                    values[j] = tmp;
                }
            }
        }

        if (count % 2 == 1) {
            return values[count / 2];
        }

        return (values[(count / 2) - 1] + values[count / 2]) / 2.0f;
    }

    void updateAggregatedReading() {
        if (_sampleCount <= 0) {
            resetValues();
            return;
        }

        float l1Values[ENERGY_BUFFER_SIZE];
        float l2Values[ENERGY_BUFFER_SIZE];
        float totalValues[ENERGY_BUFFER_SIZE];
        float wattsValues[ENERGY_BUFFER_SIZE];

        for (int i = 0; i < _sampleCount; i++) {
            l1Values[i] = _samples[i].l1;
            l2Values[i] = _samples[i].l2;
            totalValues[i] = _samples[i].total;
            wattsValues[i] = _samples[i].watts;
        }

        _irmsL1 = median(l1Values, _sampleCount);
        _irmsL2 = median(l2Values, _sampleCount);
        _irms = median(totalValues, _sampleCount);
        _watts = median(wattsValues, _sampleCount);
    }

    void resetValues() {
        _irms = 0.0f;
        _irmsL1 = 0.0f;
        _irmsL2 = 0.0f;
        _watts = 0.0f;
    }

    void clearBuffer() {
        _sampleCount = 0;
        _sampleIndex = 0;
        resetValues();
    }

    void discard(const char* reason) {
        resetValues();

        Serial.printf(
            "[ENERGY] Invalid reading discarded — reason: %s\n",
            reason
        );
    }
};