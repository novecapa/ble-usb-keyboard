#include "MobileBadgeClient.h"

#include "BleSimulatorConfig.h"

namespace {
// BLE_UUID16_DECLARE expande a un literal compuesto, que en C++ es un temporal.
const ble_uuid16_t kCccdUuid = BLE_UUID16_INIT(0x2902);
constexpr uint8_t kCccdNotifyEnable[2] = {0x01, 0x00};
constexpr size_t kMaxNotifyBytes = 64;
}  // namespace

MobileBadgeClient* MobileBadgeClient::instance_ = nullptr;

MobileBadgeClient::MobileBadgeClient(PayloadStorage& storage) : storage_(storage) {
    instance_ = this;
}

void MobileBadgeClient::begin() {
    if (!SimulatorConfig::kEnablePhoneGattClient) {
        Serial.println("[gatt-client] disabled by config");
        return;
    }

    const int rc = ble_gap_event_listener_register(&gapListener_, gapEventListener, this);
    listenerRegistered_ = (rc == 0);
    Serial.printf("[gatt-client] gap event listener registered=%s rc=%d\n",
                  listenerRegistered_ ? "true" : "false",
                  rc);
    setRuntimeState(RuntimeState::kIdle, "waiting for central to connect");
}

void MobileBadgeClient::setPayloadHandler(PayloadHandler handler) {
    onFinalPayload_ = std::move(handler);
}

void MobileBadgeClient::loop() {
    if (connHandle_ == BLE_HS_CONN_HANDLE_NONE || stallReported_) {
        return;
    }

    const bool inProgress = runtimeState_ != RuntimeState::kIdle &&
                            runtimeState_ != RuntimeState::kAuthenticated &&
                            runtimeState_ != RuntimeState::kFailed;
    if (!inProgress) {
        return;
    }

    if (millis() - stateEnteredAtMs_ > SimulatorConfig::kHandshakeStallReportMs) {
        stallReported_ = true;
        Serial.printf("[gatt-client] handshake stalled in state=%d; central cerrara la conexion a los %u ms\n",
                      static_cast<int>(runtimeState_),
                      static_cast<unsigned>(SimulatorConfig::kCentralConnectionTimeoutMs));
    }
}

void MobileBadgeClient::onCentralConnected(uint16_t connHandle) {
    if (!SimulatorConfig::kEnablePhoneGattClient) {
        return;
    }

    resetSession();
    connHandle_ = connHandle;
    stallReported_ = false;
    Serial.printf("[gap] central connected\n[gap] connection_handle=%u\n", connHandle);

    setRuntimeState(RuntimeState::kExchangingMtu, "exchanging MTU on existing connection");
    const int rc = ble_gattc_exchange_mtu(connHandle_, onMtuExchanged, this);
    if (rc != 0) {
        // BLE_HS_EALREADY significa que el MTU ya se negocio: seguimos igualmente.
        Serial.printf("[gatt-client] mtu exchange not started rc=%d; continuando con MTU actual\n", rc);
        startServiceDiscovery();
    }
}

void MobileBadgeClient::onCentralDisconnected(uint16_t connHandle) {
    if (connHandle_ != connHandle) {
        return;
    }
    Serial.println("[gap] central disconnected; sesion GATT client descartada");
    resetSession();
}

int MobileBadgeClient::onMtuExchanged(uint16_t connHandle,
                                      const struct ble_gatt_error* error,
                                      uint16_t mtu,
                                      void* arg) {
    MobileBadgeClient* self = static_cast<MobileBadgeClient*>(arg);
    Serial.printf("[gatt-client] mtu exchange status=%d mtu=%u\n",
                  error != nullptr ? error->status : -1,
                  mtu);
    self->startServiceDiscovery();
    return 0;
}

void MobileBadgeClient::startServiceDiscovery() {
    setRuntimeState(RuntimeState::kDiscoveringService, "discovering central service");
    Serial.println("[gatt-client] discovering central service on existing connection");

    const NimBLEUUID serviceUuid(SimulatorConfig::kPhoneServiceUuid);
    const int rc = ble_gattc_disc_svc_by_uuid(connHandle_,
                                              &serviceUuid.getNative()->u,
                                              onServiceDiscovered,
                                              this);
    if (rc != 0) {
        fail("no se pudo iniciar el descubrimiento de servicio");
    }
}

int MobileBadgeClient::onServiceDiscovered(uint16_t connHandle,
                                           const struct ble_gatt_error* error,
                                           const struct ble_gatt_svc* service,
                                           void* arg) {
    MobileBadgeClient* self = static_cast<MobileBadgeClient*>(arg);

    if (error != nullptr && error->status == 0 && service != nullptr) {
        self->serviceStartHandle_ = service->start_handle;
        self->serviceEndHandle_ = service->end_handle;
        Serial.printf("[gatt-client] custom service found handles=%u..%u\n",
                      service->start_handle,
                      service->end_handle);
        return 0;
    }

    if (error != nullptr && error->status == BLE_HS_EDONE) {
        if (self->serviceStartHandle_ == 0) {
            self->fail("custom service not present in central GATT");
            return 0;
        }
        self->startCharacteristicDiscovery();
        return 0;
    }

    self->fail("error descubriendo el servicio");
    return 0;
}

void MobileBadgeClient::startCharacteristicDiscovery() {
    setRuntimeState(RuntimeState::kDiscoveringCharacteristic, "discovering write characteristic");

    const NimBLEUUID characteristicUuid(SimulatorConfig::kPhoneCharacteristicUuid);
    const int rc = ble_gattc_disc_chrs_by_uuid(connHandle_,
                                               serviceStartHandle_,
                                               serviceEndHandle_,
                                               &characteristicUuid.getNative()->u,
                                               onCharacteristicDiscovered,
                                               this);
    if (rc != 0) {
        fail("no se pudo iniciar el descubrimiento de caracteristica");
    }
}

int MobileBadgeClient::onCharacteristicDiscovered(uint16_t connHandle,
                                                  const struct ble_gatt_error* error,
                                                  const struct ble_gatt_chr* chr,
                                                  void* arg) {
    MobileBadgeClient* self = static_cast<MobileBadgeClient*>(arg);

    if (error != nullptr && error->status == 0 && chr != nullptr) {
        self->charValueHandle_ = chr->val_handle;
        Serial.printf("[gatt-client] characteristic found val_handle=%u properties=0x%02X\n",
                      chr->val_handle,
                      chr->properties);
        return 0;
    }

    if (error != nullptr && error->status == BLE_HS_EDONE) {
        if (self->charValueHandle_ == 0) {
            self->fail("write characteristic not present in central GATT");
            return 0;
        }
        self->startDescriptorDiscovery();
        return 0;
    }

    self->fail("error descubriendo la caracteristica");
    return 0;
}

void MobileBadgeClient::startDescriptorDiscovery() {
    setRuntimeState(RuntimeState::kDiscoveringDescriptor, "discovering CCCD");

    const int rc = ble_gattc_disc_all_dscs(connHandle_,
                                           charValueHandle_,
                                           serviceEndHandle_,
                                           onDescriptorDiscovered,
                                           this);
    if (rc != 0) {
        fail("no se pudo iniciar el descubrimiento de descriptores");
    }
}

int MobileBadgeClient::onDescriptorDiscovered(uint16_t connHandle,
                                              const struct ble_gatt_error* error,
                                              uint16_t chrValHandle,
                                              const struct ble_gatt_dsc* dsc,
                                              void* arg) {
    MobileBadgeClient* self = static_cast<MobileBadgeClient*>(arg);

    if (error != nullptr && error->status == 0 && dsc != nullptr) {
        if (ble_uuid_cmp(&dsc->uuid.u, &kCccdUuid.u) == 0) {
            self->cccdHandle_ = dsc->handle;
            Serial.printf("[gatt-client] CCCD found handle=%u\n", dsc->handle);
        }
        return 0;
    }

    if (error != nullptr && error->status == BLE_HS_EDONE) {
        if (self->cccdHandle_ == 0) {
            // Sin CCCD no hay notify, y el central solo responde por notify.
            self->fail("CCCD ausente: el central no podria notificar");
            return 0;
        }
        self->subscribeToNotifications();
        return 0;
    }

    self->fail("error descubriendo el CCCD");
    return 0;
}

void MobileBadgeClient::subscribeToNotifications() {
    setRuntimeState(RuntimeState::kSubscribing, "enabling notifications");

    const int rc = ble_gattc_write_flat(connHandle_,
                                        cccdHandle_,
                                        kCccdNotifyEnable,
                                        sizeof(kCccdNotifyEnable),
                                        onSubscribeComplete,
                                        this);
    if (rc != 0) {
        fail("no se pudo escribir el CCCD");
    }
}

int MobileBadgeClient::onSubscribeComplete(uint16_t connHandle,
                                           const struct ble_gatt_error* error,
                                           struct ble_gatt_attr* attr,
                                           void* arg) {
    MobileBadgeClient* self = static_cast<MobileBadgeClient*>(arg);

    if (error == nullptr || error->status != 0) {
        self->fail("el central rechazo la suscripcion al CCCD");
        return 0;
    }

    Serial.println("[gatt-client] subscribed to notifications");
    self->startStep1();
    return 0;
}

void MobileBadgeClient::startStep1() {
    const std::vector<uint8_t> firstWrite = parser_.beginSession();
    setRuntimeState(RuntimeState::kWaitingStep1, "waiting AES(rand_twn) notify");
    Serial.printf("[protocol] sending step 1 rand_twn=%s\n",
                  PayloadStorage::toHex(firstWrite).c_str());
    storage_.recordStep("write-step1", firstWrite);

    if (!writeToPhone(firstWrite)) {
        fail("no se pudo escribir el paso 1");
    }
}

int MobileBadgeClient::gapEventListener(struct ble_gap_event* event, void* arg) {
    MobileBadgeClient* self = static_cast<MobileBadgeClient*>(arg);

    if (event->type != BLE_GAP_EVENT_NOTIFY_RX) {
        return 0;
    }
    if (event->notify_rx.conn_handle != self->connHandle_ ||
        event->notify_rx.attr_handle != self->charValueHandle_) {
        return 0;
    }

    uint16_t length = OS_MBUF_PKTLEN(event->notify_rx.om);
    if (length > kMaxNotifyBytes) {
        length = kMaxNotifyBytes;
    }

    uint8_t buffer[kMaxNotifyBytes];
    if (os_mbuf_copydata(event->notify_rx.om, 0, length, buffer) != 0) {
        self->fail("no se pudo leer el mbuf de la notificacion");
        return 0;
    }

    self->handleNotify(buffer, length);
    return 0;
}

void MobileBadgeClient::handleNotify(const uint8_t* data, size_t length) {
    Serial.printf("[protocol] notification received len=%u data=%s\n",
                  static_cast<unsigned>(length),
                  PayloadStorage::toHex(data, length).c_str());
    storage_.recordStep("notify", std::vector<uint8_t>(data, data + length));

    const ProtocolParser::TransitionResult result = parser_.handleNotification(data, length);
    Serial.printf("[protocol] %s\n", result.message);

    if (result.completed) {
        setRuntimeState(RuntimeState::kAuthenticated, "final payload received");
        storage_.recordFinalPayload(result.nextWrite);
        Serial.printf("[protocol] mobile badge=%s\n",
                      PayloadStorage::toHex(result.nextWrite).c_str());
        // Reenvia el payload final a la salida de teclado USB HID. El handler
        // solo encola (no bloquea): el tecleo real ocurre fuera del callback.
        if (onFinalPayload_) {
            onFinalPayload_(result.nextWrite.data(), result.nextWrite.size());
        }
        return;
    }

    if (!result.shouldWrite) {
        if (parser_.state() == ProtocolParser::SessionState::kFailed) {
            fail(result.message);
        }
        return;
    }

    switch (parser_.state()) {
        case ProtocolParser::SessionState::kWaitingPhoneRandom:
            setRuntimeState(RuntimeState::kWaitingStep2, "waiting rand_phone notify");
            break;
        case ProtocolParser::SessionState::kWaitingFinalPayload:
            setRuntimeState(RuntimeState::kWaitingPayload, "waiting mobile badge notify");
            break;
        default:
            break;
    }

    storage_.recordStep("write-next", result.nextWrite);
    if (!writeToPhone(result.nextWrite)) {
        fail("fallo escribiendo el siguiente paso");
    }
}

bool MobileBadgeClient::writeToPhone(const std::vector<uint8_t>& bytes) {
    const int rc = ble_gattc_write_flat(connHandle_,
                                        charValueHandle_,
                                        bytes.data(),
                                        bytes.size(),
                                        onWriteComplete,
                                        this);
    Serial.printf("[gatt-client] write len=%u rc=%d\n",
                  static_cast<unsigned>(bytes.size()),
                  rc);
    return rc == 0;
}

int MobileBadgeClient::onWriteComplete(uint16_t connHandle,
                                       const struct ble_gatt_error* error,
                                       struct ble_gatt_attr* attr,
                                       void* arg) {
    MobileBadgeClient* self = static_cast<MobileBadgeClient*>(arg);
    const int status = error != nullptr ? error->status : -1;
    if (status != 0) {
        Serial.printf("[gatt-client] write failed status=%d\n", status);
        self->fail("el central rechazo la escritura");
    }
    return 0;
}

void MobileBadgeClient::fail(const char* reason) {
    setRuntimeState(RuntimeState::kFailed, reason);
    Serial.printf("[gatt-client] FAIL: %s\n", reason);
}

void MobileBadgeClient::resetSession() {
    parser_.reset();
    connHandle_ = BLE_HS_CONN_HANDLE_NONE;
    serviceStartHandle_ = 0;
    serviceEndHandle_ = 0;
    charValueHandle_ = 0;
    cccdHandle_ = 0;
    stallReported_ = false;
    setRuntimeState(RuntimeState::kIdle, "session reset");
}

void MobileBadgeClient::setRuntimeState(RuntimeState state, const char* reason) {
    runtimeState_ = state;
    stateEnteredAtMs_ = millis();
    Serial.printf("[gatt-client-state] t=%lu state=%d reason=%s\n",
                  static_cast<unsigned long>(stateEnteredAtMs_),
                  static_cast<int>(runtimeState_),
                  reason);
}
