#pragma once

#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// Capacidad de USB HID nativo a nivel de compilacion
// ---------------------------------------------------------------------------
//
// Dos condiciones independientes deben cumplirse:
//
// 1. El SoC debe tener periferico USB-OTG. Solo ESP32-S2 y ESP32-S3 lo tienen.
//    Un ESP32 clasico (ESP32-D0WD y familia) no dispone de USB nativo: su
//    puerto "USB" es un puente serie (CP210x / CH340) incapaz de enumerar como
//    teclado HID. No hay solucion por software para ese caso.
//
// 2. El nucleo Arduino debe compilarse con la pila TinyUSB, es decir con
//    ARDUINO_USB_MODE=0. Con ARDUINO_USB_MODE=1 el nucleo usa el periferico
//    USB-Serial-JTAG por hardware (HWCDC): no se compila CONFIG_TINYUSB_ENABLED
//    y ni USB.h ni USBHIDKeyboard.h existen para el proyecto.
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define DCTECH_USB_NATIVE_SOC 1
#else
#define DCTECH_USB_NATIVE_SOC 0
#endif

#if DCTECH_USB_NATIVE_SOC && defined(ARDUINO_USB_MODE) && (ARDUINO_USB_MODE == 0)
#define DCTECH_USB_HID_AVAILABLE 1
#else
#define DCTECH_USB_HID_AVAILABLE 0
#endif

// Estado real de la salida de teclado USB HID. Distingue las causas para poder
// registrarlas en los logs en lugar de un generico "no disponible".
enum class UsbKeyboardStatus {
    kUnsupportedHardware,     // el SoC no tiene USB-OTG (definitivo)
    kDisabledByBuildConfig,   // TinyUSB HID no compilado, ARDUINO_USB_MODE!=0 (definitivo)
    kNotInitialized,          // begin() todavia no se ha llamado (o fallo)
    kWaitingForHost,          // USB iniciado pero el host aun no lo ha enumerado
    kReady,                   // enumerado y endpoint HID listo para enviar reportes
};

// Motivo legible para logs (`reason=...`). Nunca devuelve nullptr.
const char* usbKeyboardStatusReason(UsbKeyboardStatus status);

// true cuando el estado no puede mejorar esperando: el hardware o la
// configuracion de compilacion excluyen el HID. Reintentar no tiene sentido.
bool usbKeyboardStatusIsTerminal(UsbKeyboardStatus status);

// Abstraccion de la salida de teclado USB HID.
//
// Responsabilidad unica: inicializar el HID, informar de su estado real y
// teclear texto. No conoce nada de BLE ni de la transformacion del payload.
class UsbKeyboardOutput {
  public:
    // Inicializa el subsistema USB HID en el orden que exige el nucleo Arduino:
    // primero el descriptor del teclado, despues el arranque de la pila USB.
    // Devuelve true si la pila quedo arrancada; que el host la haya enumerado
    // es una condicion posterior que se consulta con status().
    bool begin();

    // Estado consultado en vivo (enumeracion USB y endpoint HID incluidos).
    UsbKeyboardStatus status() const;

    // Atajo de status() == kReady.
    bool isReady() const;

    // Teclea la cadena caracter a caracter. Si appendEnter es true, anade
    // una pulsacion Enter al final. Devuelve false si el teclado no esta
    // disponible o si algun caracter no pudo enviarse.
    bool typeText(const std::string& text, bool appendEnter);

  private:
    bool started_ = false;
};
