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

// Fuerza la representacion hexadecimal del payload aunque sus bytes formen
// texto imprimible. El payload final es un identificador binario: teclear
// "AB" en lugar de "4142" seria una interpretacion ASCII no deseada.
// Con true, 01 23 45 67 89 AB CD EF -> "0123456789ABCDEF" siempre.
static constexpr bool kAlwaysEncodePayloadAsHex = true;

// Anexa un salto de linea (tecla Enter) al final de cada payload tecleado.
// Por defecto false: el host recibe solo los caracteres del payload.
static constexpr bool kAppendEnterAfterPayload = false;

// Tamano maximo de payload BLE aceptado para la salida de teclado. Los payloads
// mas grandes se descartan con un log de aviso.
static constexpr size_t kMaximumBlePayloadSize = 256;

// Retardo entre caracteres al teclear por USB HID, para dar tiempo al host.
static constexpr uint32_t kKeyboardCharacterDelayMs = 5;

// Numero de payloads que la cola de teclado puede almacenar en espera.
static constexpr size_t kKeyboardQueueDepth = 8;

// Tiempo maximo que un payload se mantiene retenido esperando a que el host
// enumere el teclado USB. Pasado ese margen se descarta con un log explicito,
// de modo que no queda un reintento indefinido. No aplica cuando el hardware
// o la configuracion de compilacion descartan el HID de forma definitiva:
// en ese caso el payload se descarta de inmediato con el motivo real.
static constexpr uint32_t kUsbReadyTimeoutMs = 20000;

// Periodo minimo entre logs de reintento mientras se espera a USB, para no
// inundar el monitor serie.
static constexpr uint32_t kUsbRetryLogIntervalMs = 2000;

// Evita teclear dos veces el mismo payload dentro de una misma sesion BLE
// (reenvios o notificaciones duplicadas). El filtro se reinicia al terminar la
// conexion, de modo que una sesion nueva puede volver a teclear el mismo valor.
static constexpr bool kDeduplicatePayloadPerBleSession = true;

}
