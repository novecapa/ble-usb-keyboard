#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "BleSimulatorConfig.h"
#include "UsbKeyboardOutput.h"

// Puente entre el payload recibido por BLE y la salida de teclado USB HID.
//
// Desacopla el contexto del callback BLE del trabajo pesado de teclear:
//
//   callback BLE  -> enqueuePayload()  (copia rapida, no bloqueante)
//        loop()   -> process()         (convierte, reintenta y teclea por HID)
//
// La transferencia usa una cola de FreeRTOS que copia el payload por valor, de
// modo que no hay estado mutable compartido ni punteros sin owner entre tareas.
// Todo el estado de tecleo (payload retenido, filtro de duplicados) lo toca en
// exclusiva la tarea de loop(); lo unico que el callback BLE puede modificar es
// una bandera atomica de fin de sesion.
class KeyboardBridge {
  public:
    bool begin();

    // Invocado desde el contexto del callback BLE. Valida y encola el payload
    // sin bloquear. Devuelve false si esta deshabilitado, el payload esta vacio,
    // excede el tamano maximo o la cola esta llena.
    bool enqueuePayload(const uint8_t* data, size_t length);

    // Invocado desde loop(). Atiende el payload retenido y drena la cola.
    void process();

    // Invocado desde el callback BLE de desconexion. Solo marca una bandera:
    // el reinicio real del filtro de duplicados ocurre en process().
    void onBleSessionEnded();

  private:
    // Elemento de la cola: buffer de tamano fijo copiado por valor. Evita
    // asignaciones dinamicas y punteros compartidos entre el callback BLE y
    // loop(). La longitud es explicita: el payload puede contener 0x00 y nunca
    // se trata como cadena terminada en nulo.
    struct PayloadItem {
        size_t length = 0;
        uint8_t bytes[SimulatorConfig::kMaximumBlePayloadSize] = {};
    };

    // Intenta teclear el payload retenido. Si USB no esta listo lo mantiene y
    // reintenta en la siguiente llamada, hasta agotar kUsbReadyTimeoutMs.
    void servicePendingPayload();

    // Reinicia el filtro de duplicados cuando la sesion BLE ha terminado y ya
    // no queda nada por teclear. Hacerlo solo en reposo evita que un reinicio
    // de sesion reabra la puerta a un duplicado todavia en cola.
    void applySessionResetIfIdle();

    bool isDuplicateOfLastTyped(const PayloadItem& item) const;

    UsbKeyboardOutput keyboard_;
    QueueHandle_t queue_ = nullptr;
    bool enabled_ = false;

    PayloadItem pending_{};
    bool hasPending_ = false;
    uint32_t pendingSinceMs_ = 0;
    uint32_t lastRetryLogMs_ = 0;
    bool retryLogged_ = false;

    PayloadItem lastTyped_{};
    bool hasLastTyped_ = false;

    std::atomic<bool> sessionResetRequested_{false};
};
