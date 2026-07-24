#pragma once

#include <NimBLEDevice.h>

#include <functional>

#include "PayloadStorage.h"

class BlePeripheralServer {
  public:
    using ConnectionHandler = std::function<void(uint16_t connHandle)>;

    explicit BlePeripheralServer(PayloadStorage& storage);
    void begin();
    void loop();

    // El handle de la conexion entrante es el unico punto de entrada al GATT
    // del central: la app central no anuncia su servicio, asi que no hay forma
    // de alcanzarlo escaneando.
    void setConnectionHandlers(ConnectionHandler onConnected, ConnectionHandler onDisconnected);

  private:
    class ServerCallbacks : public NimBLEServerCallbacks {
      public:
        explicit ServerCallbacks(BlePeripheralServer& owner) : owner_(owner) {}
        void onConnect(NimBLEServer* server, ble_gap_conn_desc* desc) override;
        void onDisconnect(NimBLEServer* server, ble_gap_conn_desc* desc) override;

      private:
        BlePeripheralServer& owner_;
    };

    void startAdvertising();

    PayloadStorage& storage_;
    NimBLEServer* server_ = nullptr;
    ServerCallbacks callbacks_;
    ConnectionHandler onConnected_;
    ConnectionHandler onDisconnected_;
    bool advertisingRestartPending_ = false;
    uint32_t advertisingRestartAtMs_ = 0;
};
