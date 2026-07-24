#include "UsbKeyboardOutput.h"

#include <Arduino.h>

#include "BleSimulatorConfig.h"

const char* usbKeyboardStatusReason(UsbKeyboardStatus status) {
    switch (status) {
        case UsbKeyboardStatus::kUnsupportedHardware:
            return "soc_has_no_native_usb";
        case UsbKeyboardStatus::kDisabledByBuildConfig:
            return "tinyusb_hid_not_compiled_arduino_usb_mode_not_0";
        case UsbKeyboardStatus::kNotInitialized:
            return "usb_not_initialized";
        case UsbKeyboardStatus::kWaitingForHost:
            return "usb_not_enumerated_by_host";
        case UsbKeyboardStatus::kReady:
            return "ready";
    }
    return "unknown";
}

bool usbKeyboardStatusIsTerminal(UsbKeyboardStatus status) {
    return status == UsbKeyboardStatus::kUnsupportedHardware ||
           status == UsbKeyboardStatus::kDisabledByBuildConfig;
}

bool UsbKeyboardOutput::isReady() const {
    return status() == UsbKeyboardStatus::kReady;
}

#if DCTECH_USB_HID_AVAILABLE

#include "USB.h"
#include "USBHIDKeyboard.h"

namespace {

// Instancia unica del teclado. Su constructor registra la interfaz HID en
// TinyUSB, por eso debe existir antes de que USB.begin() construya el
// descriptor de configuracion.
USBHIDKeyboard gKeyboard;

}  // namespace

bool UsbKeyboardOutput::begin() {
    if (started_) {
        return true;
    }

    // Orden obligatorio: USBHIDKeyboard::begin() prepara los semaforos del
    // endpoint HID y debe ejecutarse ANTES de arrancar la pila; USB.begin()
    // publica ya el descriptor completo al host.
    gKeyboard.begin();
    if (!USB.begin()) {
        Serial.println("[usb-hid] USB.begin() failed; keyboard stack not started");
        return false;
    }

    started_ = true;
    Serial.println("[usb-hid] USB HID stack started; waiting for host enumeration");
    return true;
}

UsbKeyboardStatus UsbKeyboardOutput::status() const {
    if (!started_) {
        return UsbKeyboardStatus::kNotInitialized;
    }
    // ESPUSB::operator bool() == (pila arrancada && dispositivo montado por el
    // host). Es la unica senal fiable de enumeracion en el nucleo Arduino 2.x.
    if (!static_cast<bool>(USB)) {
        return UsbKeyboardStatus::kWaitingForHost;
    }
    // El endpoint HID puede estar ocupado con un reporte en vuelo; equivale a
    // "aun no se puede escribir", no a un fallo.
    if (!tud_hid_n_ready(0)) {
        return UsbKeyboardStatus::kWaitingForHost;
    }
    return UsbKeyboardStatus::kReady;
}

bool UsbKeyboardOutput::typeText(const std::string& text, bool appendEnter) {
    const UsbKeyboardStatus current = status();
    if (current != UsbKeyboardStatus::kReady) {
        Serial.printf("[usb-hid] type request ignored reason=%s\n",
                      usbKeyboardStatusReason(current));
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

// Stub para targets sin USB HID: ESP32 clasico (sin USB-OTG) o build con
// ARDUINO_USB_MODE distinto de 0 (sin TinyUSB). No emula un teclado por Serial:
// informa del motivo real y mantiene el resto del firmware operativo.
bool UsbKeyboardOutput::begin() {
    started_ = false;
    Serial.printf("[usb-hid] keyboard output disabled reason=%s\n",
                  usbKeyboardStatusReason(status()));
    return false;
}

UsbKeyboardStatus UsbKeyboardOutput::status() const {
#if DCTECH_USB_NATIVE_SOC
    return UsbKeyboardStatus::kDisabledByBuildConfig;
#else
    return UsbKeyboardStatus::kUnsupportedHardware;
#endif
}

bool UsbKeyboardOutput::typeText(const std::string& /*text*/, bool /*appendEnter*/) {
    return false;
}

#endif  // DCTECH_USB_HID_AVAILABLE
