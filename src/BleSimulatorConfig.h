#pragma once

#include <cstddef>
#include <cstdint>

namespace SimulatorConfig {

// Nombre anunciado por el periferico BLE.
static constexpr char kAdvertisedName[] = "DCTECH";

// ---------------------------------------------------------------------------
// UUID del servicio GATT (valores internos de 128 bits)
// ---------------------------------------------------------------------------

static constexpr char kPhoneServiceUuid[] = "fe029319-5f83-447a-8f1c-c7c19fa19201";
static constexpr char kPhoneCharacteristicUuid[] = "fe029319-5f83-447a-8f1c-c7c19fa19202";

// Clave de laboratorio AES-128 (32 caracteres hex = 128 bits).
// NO es un mecanismo de seguridad de produccion: reemplazala en cualquier uso real.
static constexpr char kMasterKeyHex[] = "12f9bab90a7a28e969193ef5c699564e";

// ---------------------------------------------------------------------------
// Manufacturer data (identificadores internos de simulacion)
// ---------------------------------------------------------------------------

// IDs internos de laboratorio del simulador DCTECH. Estan en el rango alto
// (0xFFxx) para no colisionar con Company Identifiers oficiales de Bluetooth SIG.
static constexpr uint16_t kPrimaryManufacturerId = 0xFFF0;
static constexpr uint16_t kSecondaryManufacturerId = 0xFFF1;

// Campos del bloque de manufacturer data. Solo los bytes 0..1 (manufacturer ID)
// y el byte 5 (application type) son significativos para el central; el resto
// son marcadores estables del simulador.
static constexpr uint8_t kManufacturerBlockDescriptor = 0x00;
static constexpr uint8_t kManufacturerFirmwareCompat = 0x22;
static constexpr uint8_t kManufacturerModuleType = 0x01;
static constexpr uint8_t kManufacturerApplicationType = 0x01;
static constexpr uint8_t kManufacturerAppSpecificCode = 0x00;
static constexpr uint8_t kManufacturerTxPowerPlaceholder = 0x00;

// El manufacturer ID viaja en little-endian en el payload de advertising.
static constexpr uint8_t kManufacturerData[] = {
    static_cast<uint8_t>(kPrimaryManufacturerId & 0xFF),
    static_cast<uint8_t>((kPrimaryManufacturerId >> 8) & 0xFF),
    kManufacturerBlockDescriptor,
    kManufacturerFirmwareCompat,
    kManufacturerModuleType,
    kManufacturerApplicationType,
    kManufacturerAppSpecificCode,
    kManufacturerTxPowerPlaceholder,
};

static constexpr uint32_t kSerialBaudRate = 115200;
static constexpr uint32_t kAdvertiseRestartDelayMs = 250;

// El central cierra la conexion 5 s despues de conectar; el handshake completo
// debe caber dentro de esa ventana. Solo se usa para diagnostico.
static constexpr uint32_t kCentralConnectionTimeoutMs = 5000;
static constexpr uint32_t kHandshakeStallReportMs = 3000;

static constexpr bool kEnablePhoneGattClient = true;
static constexpr bool kSendStep2AsSixteenZeroes = true;

// Habilita el reenvio del payload BLE recibido como pulsaciones de teclado USB.
// Solo tiene efecto real en placas con USB nativo (ESP32-S2 / ESP32-S3);
// en un ESP32 clasico la salida queda deshabilitada de forma segura.
static constexpr bool kUsbKeyboardEnabled = true;

// Anexa un salto de linea (tecla Enter) al final de cada payload tecleado.
static constexpr bool kAppendEnterAfterPayload = true;

// Tamano maximo de payload BLE aceptado para la salida de teclado. Los payloads
// mas grandes se descartan con un log de aviso.
static constexpr size_t kMaximumBlePayloadSize = 256;

// Retardo entre caracteres al teclear por USB HID, para dar tiempo al host.
static constexpr uint32_t kKeyboardCharacterDelayMs = 5;

// Numero de payloads que la cola de teclado puede almacenar en espera.
static constexpr size_t kKeyboardQueueDepth = 8;

}
