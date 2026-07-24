#include "UsbKeyboardOutput.h"

#include <Arduino.h>

#include "BleSimulatorConfig.h"

#if DCTECH_USB_HID_AVAILABLE

#include "USB.h"
#include "USBHIDKeyboard.h"

namespace {
USBHIDKeyboard gKeyboard;
}  // namespace

bool UsbKeyboardOutput::begin() {
    if (ready_) {
        return true;
    }
    gKeyboard.begin();
    USB.begin();
    ready_ = true;
    Serial.println("[usb-hid] USB HID keyboard initialized");
    return true;
}

bool UsbKeyboardOutput::isReady() const {
    return ready_;
}

bool UsbKeyboardOutput::typeText(const std::string& text, bool appendEnter) {
    if (!ready_) {
        Serial.println("[usb-hid] type request ignored: keyboard not available");
        return false;
    }

    bool allSent = true;
    for (const char c : text) {
        if (gKeyboard.write(static_cast<uint8_t>(c)) != 1) {
            allSent = false;  // caracter no soportado por el mapa HID
        }
        if (SimulatorConfig::kKeyboardCharacterDelayMs > 0) {
            delay(SimulatorConfig::kKeyboardCharacterDelayMs);
        }
    }

    if (appendEnter) {
        gKeyboard.write(static_cast<uint8_t>(KEY_RETURN));
    }

    if (!allSent) {
        Serial.println("[usb-hid] some characters were not supported by the HID keymap");
    }
    return allSent;
}

#else  // DCTECH_USB_HID_AVAILABLE

// Stub para targets sin USB nativo (ESP32 clasico). No emula un teclado por
// Serial: simplemente informa de que el hardware no es compatible y mantiene
// el resto del firmware operativo.
bool UsbKeyboardOutput::begin() {
    ready_ = false;
    Serial.println("[usb-hid] hardware sin USB nativo: salida de teclado deshabilitada");
    return false;
}

bool UsbKeyboardOutput::isReady() const {
    return false;
}

bool UsbKeyboardOutput::typeText(const std::string& /*text*/, bool /*appendEnter*/) {
    return false;
}

#endif  // DCTECH_USB_HID_AVAILABLE
