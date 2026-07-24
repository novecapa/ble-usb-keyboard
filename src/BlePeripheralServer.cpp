#include "BlePeripheralServer.h"

#include "BleSimulatorConfig.h"

BlePeripheralServer::BlePeripheralServer(PayloadStorage& storage)
    : storage_(storage), callbacks_(*this) {}

void BlePeripheralServer::setConnectionHandlers(ConnectionHandler onConnected,
                                                ConnectionHandler onDisconnected) {
    onConnected_ = std::move(onConnected);
    onDisconnected_ = std::move(onDisconnected);
}

void BlePeripheralServer::begin() {
    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(&callbacks_);
    startAdvertising();
}

void BlePeripheralServer::loop() {
    if (!advertisingRestartPending_) {
        return;
    }

    if (millis() < advertisingRestartAtMs_) {
        return;
    }

    advertisingRestartPending_ = false;
    startAdvertising();
}

void BlePeripheralServer::startAdvertising() {
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();

    NimBLEAdvertisementData advertisementData;
    advertisementData.setFlags(0x06);
    advertisementData.setManufacturerData(std::string(
        reinterpret_cast<const char*>(SimulatorConfig::kManufacturerData),
        sizeof(SimulatorConfig::kManufacturerData)));

    NimBLEAdvertisementData scanResponseData;
    scanResponseData.setName(SimulatorConfig::kAdvertisedName);

    advertising->stop();
    advertising->setAdvertisementData(advertisementData);
    advertising->setScanResponseData(scanResponseData);
    const bool started = advertising->start();

    Serial.printf("[adv] started=%s name=%s flags=06 manufacturer=%s\n",
                  started ? "true" : "false",
                  SimulatorConfig::kAdvertisedName,
                  PayloadStorage::toHex(SimulatorConfig::kManufacturerData,
                                        sizeof(SimulatorConfig::kManufacturerData))
                      .c_str());
}

void BlePeripheralServer::ServerCallbacks::onConnect(NimBLEServer* server, ble_gap_conn_desc* desc) {
    owner_.storage_.recordConnectionEvent("central connected to simulator");
    Serial.printf("[adv] connected handle=%u\n", desc->conn_handle);
    if (owner_.onConnected_) {
        owner_.onConnected_(desc->conn_handle);
    }
}

void BlePeripheralServer::ServerCallbacks::onDisconnect(NimBLEServer* server, ble_gap_conn_desc* desc) {
    owner_.storage_.recordConnectionEvent("central disconnected from simulator");
    owner_.advertisingRestartPending_ = true;
    owner_.advertisingRestartAtMs_ = millis() + SimulatorConfig::kAdvertiseRestartDelayMs;
    Serial.printf("[adv] disconnected handle=%u; advertising restart scheduled in %lu ms\n",
                  desc->conn_handle,
                  static_cast<unsigned long>(SimulatorConfig::kAdvertiseRestartDelayMs));
    if (owner_.onDisconnected_) {
        owner_.onDisconnected_(desc->conn_handle);
    }
}
