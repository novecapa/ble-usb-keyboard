#include "PayloadConverter.h"

namespace PayloadConverter {
namespace {

constexpr char kHexDigits[] = "0123456789ABCDEF";

// Comprueba si el buffer es texto UTF-8 valido formado solo por caracteres
// imprimibles (ASCII imprimible mas cualquier secuencia UTF-8 multibyte bien
// formada). Los controles no imprimibles distintos de nada relevante hacen que
// el payload se trate como binario.
bool isPrintableUtf8(const uint8_t* data, size_t length) {
    size_t i = 0;
    while (i < length) {
        const uint8_t byte = data[i];

        if (byte < 0x80) {
            // ASCII: solo aceptamos imprimibles (0x20..0x7E).
            if (byte < 0x20 || byte == 0x7F) {
                return false;
            }
            ++i;
            continue;
        }

        // Secuencia UTF-8 multibyte: determina su longitud.
        size_t sequenceLength = 0;
        if ((byte & 0xE0) == 0xC0) {
            sequenceLength = 2;
        } else if ((byte & 0xF0) == 0xE0) {
            sequenceLength = 3;
        } else if ((byte & 0xF8) == 0xF0) {
            sequenceLength = 4;
        } else {
            return false;  // byte inicial invalido
        }

        if (i + sequenceLength > length) {
            return false;  // secuencia truncada
        }
        for (size_t k = 1; k < sequenceLength; ++k) {
            if ((data[i + k] & 0xC0) != 0x80) {
                return false;  // byte de continuacion invalido
            }
        }
        i += sequenceLength;
    }
    return true;
}

std::string toHex(const uint8_t* data, size_t length) {
    std::string out;
    out.reserve(length * 2);
    for (size_t i = 0; i < length; ++i) {
        out.push_back(kHexDigits[(data[i] >> 4) & 0x0F]);
        out.push_back(kHexDigits[data[i] & 0x0F]);
    }
    return out;
}

}  // namespace

Result convert(const uint8_t* data, size_t length) {
    Result result;
    if (data == nullptr || length == 0) {
        result.encoding = Encoding::kText;
        return result;
    }

    if (isPrintableUtf8(data, length)) {
        result.encoding = Encoding::kText;
        result.text.assign(reinterpret_cast<const char*>(data), length);
        return result;
    }

    result.encoding = Encoding::kHex;
    result.text = toHex(data, length);
    return result;
}

}  // namespace PayloadConverter
