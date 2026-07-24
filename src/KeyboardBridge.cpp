#include "KeyboardBridge.h"

#include <Arduino.h>

#include "BleSimulatorConfig.h"
#include "PayloadConverter.h"

namespace {

// Elemento de la cola: buffer de tamano fijo copiado por valor. Evita
// asignaciones dinamicas y punteros compartidos entre el callback BLE y loop().
struct PayloadItem {
    size_t length = 0;
    uint8_t bytes[SimulatorConfig::kMaximumBlePayloadSize];
};

}  // namespace

bool KeyboardBridge::begin() {
    if (!SimulatorConfig::kUsbKeyboardEnabled) {
        Serial.println("[keyboard-bridge] USB keyboard disabled by config");
        enabled_ = false;
        return false;
    }

    queue_ = xQueueCreate(SimulatorConfig::kKeyboardQueueDepth, sizeof(PayloadItem));
    if (queue_ == nullptr) {
        Serial.println("[keyboard-bridge] failed to create payload queue");
        enabled_ = false;
        return false;
    }

    enabled_ = true;
    const bool keyboardReady = keyboard_.begin();
    Serial.printf("[keyboard-bridge] ready; usb_hid_available=%s\n",
                  keyboardReady ? "true" : "false");
    return true;
}

bool KeyboardBridge::enqueuePayload(const uint8_t* data, size_t length) {
    if (!enabled_ || queue_ == nullptr) {
        return false;
    }
    if (data == nullptr || length == 0) {
        Serial.println("[keyboard-bridge] empty payload ignored");
        return false;
    }
    if (length > SimulatorConfig::kMaximumBlePayloadSize) {
        Serial.printf("[keyboard-bridge] payload too large len=%u max=%u; dropped\n",
                      static_cast<unsigned>(length),
                      static_cast<unsigned>(SimulatorConfig::kMaximumBlePayloadSize));
        return false;
    }

    PayloadItem item;
    item.length = length;
    memcpy(item.bytes, data, length);

    if (xQueueSend(queue_, &item, 0) != pdTRUE) {
        Serial.println("[keyboard-bridge] queue full; payload dropped");
        return false;
    }

    Serial.printf("[keyboard-bridge] payload queued len=%u\n",
                  static_cast<unsigned>(length));
    return true;
}

void KeyboardBridge::process() {
    if (!enabled_ || queue_ == nullptr) {
        return;
    }

    PayloadItem item;
    while (xQueueReceive(queue_, &item, 0) == pdTRUE) {
        const PayloadConverter::Result converted =
            PayloadConverter::convert(item.bytes, item.length);
        const char* encoding =
            converted.encoding == PayloadConverter::Encoding::kText ? "text" : "hex";
        Serial.printf("[keyboard-bridge] typing payload len=%u encoding=%s out_len=%u\n",
                      static_cast<unsigned>(item.length),
                      encoding,
                      static_cast<unsigned>(converted.text.size()));

        if (!keyboard_.isReady()) {
            Serial.println("[keyboard-bridge] USB keyboard not available; payload not typed");
            continue;
        }

        const bool typed =
            keyboard_.typeText(converted.text, SimulatorConfig::kAppendEnterAfterPayload);
        Serial.printf("[keyboard-bridge] typed=%s\n", typed ? "ok" : "partial");
    }
}
