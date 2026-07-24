#pragma once

#include <NimBLEDevice.h>

#if defined(CONFIG_NIMBLE_CPP_IDF)
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#else
#include "nimble/nimble/host/include/host/ble_gatt.h"
#include "nimble/nimble/host/include/host/ble_hs.h"
#endif

#include <functional>

#include "PayloadStorage.h"
#include "ProtocolParser.h"

// Cliente GATT del servicio personalizado que publica la app central.
//
// La app central es GAP Central y GATT Server a la vez: nunca anuncia su
// servicio, asi que este cliente no puede descubrirlo por escaneo ni abrir una
// segunda conexion. El unico camino es reutilizar el conn_handle de la conexion
// que el central ya ha establecido contra nuestro advertising DCTECH.
// NimBLEClient no lo permite (m_conn_id es privado y connect() siempre inicia un
// enlace nuevo), de modo que aqui se usan directamente las primitivas
// ble_gattc_* del host Apache NimBLE, que aceptan cualquier conn_handle con
// independencia del rol GAP.
class MobileBadgeClient {
  public:
    // Notifica el payload final recibido por BLE (data, length). Se usa para
    // reenviarlo a la salida de teclado USB HID sin acoplar BLE con USB.
    using PayloadHandler = std::function<void(const uint8_t* data, size_t length)>;

    enum class RuntimeState {
        kIdle,
        kExchangingMtu,
        kDiscoveringService,
        kDiscoveringCharacteristic,
        kDiscoveringDescriptor,
        kSubscribing,
        kWaitingStep1,
        kWaitingStep2,
        kWaitingPayload,
        kAuthenticated,
        kFailed
    };

    explicit MobileBadgeClient(PayloadStorage& storage);
    void begin();
    void loop();

    // Registra el receptor del payload final (p.ej. la salida de teclado USB).
    void setPayloadHandler(PayloadHandler handler);

    // Invocados por BlePeripheralServer con el handle de la conexion entrante.
    void onCentralConnected(uint16_t connHandle);
    void onCentralDisconnected(uint16_t connHandle);

  private:
    static int gapEventListener(struct ble_gap_event* event, void* arg);
    static int onMtuExchanged(uint16_t connHandle,
                              const struct ble_gatt_error* error,
                              uint16_t mtu,
                              void* arg);
    static int onServiceDiscovered(uint16_t connHandle,
                                   const struct ble_gatt_error* error,
                                   const struct ble_gatt_svc* service,
                                   void* arg);
    static int onCharacteristicDiscovered(uint16_t connHandle,
                                          const struct ble_gatt_error* error,
                                          const struct ble_gatt_chr* chr,
                                          void* arg);
    static int onDescriptorDiscovered(uint16_t connHandle,
                                      const struct ble_gatt_error* error,
                                      uint16_t chrValHandle,
                                      const struct ble_gatt_dsc* dsc,
                                      void* arg);
    static int onSubscribeComplete(uint16_t connHandle,
                                   const struct ble_gatt_error* error,
                                   struct ble_gatt_attr* attr,
                                   void* arg);
    static int onWriteComplete(uint16_t connHandle,
                               const struct ble_gatt_error* error,
                               struct ble_gatt_attr* attr,
                               void* arg);

    void startServiceDiscovery();
    void startCharacteristicDiscovery();
    void startDescriptorDiscovery();
    void subscribeToNotifications();
    void startStep1();
    void handleNotify(const uint8_t* data, size_t length);
    bool writeToPhone(const std::vector<uint8_t>& bytes);
    void fail(const char* reason);
    void resetSession();
    void setRuntimeState(RuntimeState state, const char* reason);

    static MobileBadgeClient* instance_;

    PayloadStorage& storage_;
    ProtocolParser parser_;
    PayloadHandler onFinalPayload_;
    struct ble_gap_event_listener gapListener_ {};
    bool listenerRegistered_ = false;
    bool stallReported_ = false;

    uint16_t connHandle_ = BLE_HS_CONN_HANDLE_NONE;
    uint16_t serviceStartHandle_ = 0;
    uint16_t serviceEndHandle_ = 0;
    uint16_t charValueHandle_ = 0;
    uint16_t cccdHandle_ = 0;

    uint32_t stateEnteredAtMs_ = 0;
    RuntimeState runtimeState_ = RuntimeState::kIdle;
};
