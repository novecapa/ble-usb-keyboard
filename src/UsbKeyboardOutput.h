#pragma once

#include <cstdint>
#include <string>

// Capacidad de USB HID nativo a nivel de compilacion.
//
// Solo los SoC con periferico USB-OTG (ESP32-S2 y ESP32-S3) pueden actuar como
// teclado USB HID real. Un ESP32 clasico no dispone de USB nativo: su puerto
// "USB" es un adaptador serie, incapaz de enumerar como teclado HID. En esos
// targets se compila una implementacion stub que deja el firmware BLE intacto.
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define DCTECH_USB_HID_AVAILABLE 1
#else
#define DCTECH_USB_HID_AVAILABLE 0
#endif

// Abstraccion de la salida de teclado USB HID.
//
// Responsabilidad unica: inicializar el HID, informar de su disponibilidad y
// teclear texto. No conoce nada de BLE ni de la transformacion del payload.
class UsbKeyboardOutput {
  public:
    // Inicializa el subsistema USB HID. Devuelve true si queda operativo.
    // En hardware no compatible devuelve false sin efectos secundarios.
    bool begin();

    // true cuando el teclado esta inicializado y disponible para teclear.
    bool isReady() const;

    // Teclea la cadena caracter a caracter. Si appendEnter es true, anade
    // una pulsacion Enter (salto de linea) al final. Devuelve false si el
    // teclado no esta disponible o si algun caracter no pudo enviarse.
    bool typeText(const std::string& text, bool appendEnter);

  private:
    bool ready_ = false;
};
