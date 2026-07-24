#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstddef>
#include <cstdint>

#include "UsbKeyboardOutput.h"

// Puente entre el payload recibido por BLE y la salida de teclado USB HID.
//
// Desacopla el contexto del callback BLE del trabajo pesado de teclear:
//
//   callback BLE  -> enqueuePayload()  (copia rapida, no bloqueante)
//        loop()   -> process()         (convierte y teclea por USB HID)
//
// La transferencia usa una cola de FreeRTOS que copia el payload por valor, de
// modo que no hay estado mutable compartido ni punteros sin owner entre tareas.
class KeyboardBridge {
  public:
    bool begin();

    // Invocado desde el contexto del callback BLE. Valida y encola el payload
    // sin bloquear. Devuelve false si esta deshabilitado, el payload esta vacio,
    // excede el tamano maximo o la cola esta llena.
    bool enqueuePayload(const uint8_t* data, size_t length);

    // Invocado desde loop(). Drena la cola, convierte cada payload y lo teclea.
    void process();

  private:
    UsbKeyboardOutput keyboard_;
    QueueHandle_t queue_ = nullptr;
    bool enabled_ = false;
};
