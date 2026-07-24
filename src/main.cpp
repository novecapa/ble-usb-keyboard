#include <Arduino.h>
#include <NimBLEDevice.h>

#include "BlePeripheralServer.h"
#include "BleSimulatorConfig.h"
#include "KeyboardBridge.h"
#include "MobileBadgeClient.h"
#include "PayloadStorage.h"
#include "ProtocolParser.h"

PayloadStorage gStorage;
BlePeripheralServer gPeripheralServer(gStorage);
MobileBadgeClient gMobileBadgeClient(gStorage);
ProtocolParser gProtocolParser;
KeyboardBridge gKeyboardBridge;

void setup() {
    Serial.begin(SimulatorConfig::kSerialBaudRate);
    delay(500);

    Serial.println();
    Serial.println("DCTECH BLE to USB Keyboard Simulator");
    Serial.println("GAP: peripheral advertising DCTECH service");
    Serial.println("GATT: client of the central's custom service over the incoming connection");
    Serial.printf("AES-128-ECB self-test: %s\n", gProtocolParser.selfTest() ? "PASS" : "FAIL");
    Serial.printf("Manufacturer data: %s\n",
                  PayloadStorage::toHex(SimulatorConfig::kManufacturerData,
                                        sizeof(SimulatorConfig::kManufacturerData))
                      .c_str());

    NimBLEDevice::init(SimulatorConfig::kAdvertisedName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // La salida de teclado USB HID nunca detiene el servidor BLE: begin() puede
    // devolver false (hardware sin USB nativo) sin afectar al resto.
    gKeyboardBridge.begin();
    gMobileBadgeClient.setPayloadHandler(
        [](const uint8_t* data, size_t length) { gKeyboardBridge.enqueuePayload(data, length); });

    gMobileBadgeClient.begin();
    gPeripheralServer.setConnectionHandlers(
        [](uint16_t connHandle) { gMobileBadgeClient.onCentralConnected(connHandle); },
        [](uint16_t connHandle) {
            gMobileBadgeClient.onCentralDisconnected(connHandle);
            // Cierra la sesion de teclado: el filtro de duplicados solo aplica
            // dentro de una misma conexion, para no bloquear pruebas sucesivas.
            gKeyboardBridge.onBleSessionEnded();
        });
    gPeripheralServer.begin();
}

void loop() {
    gPeripheralServer.loop();
    gMobileBadgeClient.loop();
    gKeyboardBridge.process();
    delay(50);
}
