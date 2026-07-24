#include "KeyboardBridge.h"

#include <Arduino.h>

#include <cstring>

#include "PayloadConverter.h"

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
    keyboard_.begin();

    const UsbKeyboardStatus status = keyboard_.status();
    if (usbKeyboardStatusIsTerminal(status)) {
        Serial.printf("[keyboard-bridge] USB HID unavailable reason=%s\n",
                      usbKeyboardStatusReason(status));
    } else {
        Serial.println("[keyboard-bridge] USB HID initialized");
        Serial.printf("[keyboard-bridge] USB HID state=%s\n", usbKeyboardStatusReason(status));
    }
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

    // Copia por valor: el buffer del callback BLE deja de ser valido en cuanto
    // esta funcion retorna.
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

void KeyboardBridge::onBleSessionEnded() {
    sessionResetRequested_.store(true, std::memory_order_relaxed);
}

void KeyboardBridge::process() {
    if (!enabled_ || queue_ == nullptr) {
        return;
    }

    applySessionResetIfIdle();
    servicePendingPayload();

    // Mientras haya un payload retenido no se saca otro de la cola: se preserva
    // el orden de llegada y nunca hay dos payloads compitiendo por el teclado.
    PayloadItem item;
    while (!hasPending_ && xQueueReceive(queue_, &item, 0) == pdTRUE) {
        if (isDuplicateOfLastTyped(item)) {
            Serial.printf("[keyboard-bridge] duplicate payload ignored len=%u (same BLE session)\n",
                          static_cast<unsigned>(item.length));
            continue;
        }

        pending_ = item;
        hasPending_ = true;
        pendingSinceMs_ = millis();
        lastRetryLogMs_ = 0;
        retryLogged_ = false;
        servicePendingPayload();
    }
}

void KeyboardBridge::servicePendingPayload() {
    if (!hasPending_) {
        return;
    }

    const UsbKeyboardStatus status = keyboard_.status();

    if (status != UsbKeyboardStatus::kReady) {
        const char* reason = usbKeyboardStatusReason(status);

        // Hardware sin USB-OTG o build sin TinyUSB: esperar no cambia nada.
        if (usbKeyboardStatusIsTerminal(status)) {
            Serial.printf("[keyboard-bridge] USB HID unavailable reason=%s; payload not typed\n",
                          reason);
            hasPending_ = false;
            return;
        }

        const uint32_t waitedMs = millis() - pendingSinceMs_;
        if (waitedMs >= SimulatorConfig::kUsbReadyTimeoutMs) {
            Serial.printf(
                "[keyboard-bridge] USB HID unavailable reason=%s; payload discarded after %u ms\n",
                reason,
                static_cast<unsigned>(waitedMs));
            hasPending_ = false;
            return;
        }

        const uint32_t nowMs = millis();
        if (!retryLogged_ ||
            (nowMs - lastRetryLogMs_) >= SimulatorConfig::kUsbRetryLogIntervalMs) {
            Serial.printf("[keyboard-bridge] USB not ready; payload retained reason=%s waited=%u ms\n",
                          reason,
                          static_cast<unsigned>(waitedMs));
            retryLogged_ = true;
            lastRetryLogMs_ = nowMs;
        }
        return;
    }

    if (retryLogged_) {
        Serial.println("[keyboard-bridge] USB ready; typing pending payload");
    }

    const PayloadConverter::Result converted = PayloadConverter::convert(
        pending_.bytes, pending_.length, SimulatorConfig::kAlwaysEncodePayloadAsHex);
    const char* encoding =
        converted.encoding == PayloadConverter::Encoding::kText ? "text" : "hex";
    Serial.printf("[keyboard-bridge] typing payload len=%u encoding=%s out_len=%u value=%s\n",
                  static_cast<unsigned>(pending_.length),
                  encoding,
                  static_cast<unsigned>(converted.text.size()),
                  converted.text.c_str());

    const bool typed =
        keyboard_.typeText(converted.text, SimulatorConfig::kAppendEnterAfterPayload);
    if (typed) {
        Serial.println("[keyboard-bridge] payload typed successfully");
    } else {
        Serial.printf("[keyboard-bridge] payload typed with errors reason=%s\n",
                      usbKeyboardStatusReason(keyboard_.status()));
    }

    // Se recuerda tambien tras un envio parcial: el payload ya salio por el
    // cable y repetirlo entero duplicaria caracteres en el host.
    lastTyped_ = pending_;
    hasLastTyped_ = true;
    hasPending_ = false;
}

void KeyboardBridge::applySessionResetIfIdle() {
    if (!sessionResetRequested_.load(std::memory_order_relaxed)) {
        return;
    }
    // Esperar a que no quede nada pendiente evita que el reinicio del filtro
    // permita teclear un duplicado que todavia estaba en la cola.
    if (hasPending_ || uxQueueMessagesWaiting(queue_) > 0) {
        return;
    }

    sessionResetRequested_.store(false, std::memory_order_relaxed);
    if (hasLastTyped_) {
        hasLastTyped_ = false;
        Serial.println("[keyboard-bridge] BLE session ended; duplicate filter reset");
    }
}

bool KeyboardBridge::isDuplicateOfLastTyped(const PayloadItem& item) const {
    if (!SimulatorConfig::kDeduplicatePayloadPerBleSession || !hasLastTyped_) {
        return false;
    }
    return item.length == lastTyped_.length &&
           memcmp(item.bytes, lastTyped_.bytes, item.length) == 0;
}
