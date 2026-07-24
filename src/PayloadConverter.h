#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Convierte un payload BLE en la cadena que se enviara por el teclado USB HID.
//
// Regla de conversion:
//   - Payload UTF-8 imprimible  -> se devuelve tal cual (texto).
//   - Payload binario / no imprimible -> se devuelve su representacion
//     hexadecimal en mayusculas y sin separadores (p.ej. 01 A4 FF 20 -> "01A4FF20").
//
// El resultado indica que estrategia se aplico para poder registrarla en logs.
namespace PayloadConverter {

enum class Encoding {
    kText,    // el payload era texto UTF-8 imprimible
    kHex,     // el payload contenia bytes binarios y se convirtio a hex
};

struct Result {
    std::string text;
    Encoding encoding = Encoding::kText;
};

// Nunca asume que el buffer termina en '\0'; opera siempre con (data, length).
Result convert(const uint8_t* data, size_t length);

}  // namespace PayloadConverter
