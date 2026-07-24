#include "PayloadStorage.h"

void PayloadStorage::recordConnectionEvent(const char* eventLabel) {
    Serial.printf("[conn] %lu ms %s\n", millis(), eventLabel);
}

void PayloadStorage::recordStep(const char* stepLabel, const std::vector<uint8_t>& bytes) {
    Serial.printf("[%s] len=%u hex=%s ascii=%s\n",
                  stepLabel,
                  static_cast<unsigned>(bytes.size()),
                  toHex(bytes).c_str(),
                  toAsciiPreview(bytes).c_str());
}

void PayloadStorage::recordFinalPayload(const std::vector<uint8_t>& bytes) {
    lastPayload_ = bytes;
    lastPayloadTimestampMs_ = millis();
    Serial.printf("[payload-final] t=%lu ms len=%u hex=%s ascii=%s\n",
                  lastPayloadTimestampMs_,
                  static_cast<unsigned>(bytes.size()),
                  toHex(bytes).c_str(),
                  toAsciiPreview(bytes).c_str());
}

const std::vector<uint8_t>& PayloadStorage::lastPayload() const {
    return lastPayload_;
}

String PayloadStorage::lastPayloadHex() const {
    return toHex(lastPayload_);
}

String PayloadStorage::lastPayloadAscii() const {
    return toAsciiPreview(lastPayload_);
}

String PayloadStorage::toHex(const uint8_t* data, size_t length) {
    String result;
    for (size_t i = 0; i < length; ++i) {
        if (i > 0) {
            result += " ";
        }
        if (data[i] < 0x10) {
            result += "0";
        }
        result += String(data[i], HEX);
    }
    result.toUpperCase();
    return result;
}

String PayloadStorage::toHex(const std::vector<uint8_t>& bytes) {
    return toHex(bytes.data(), bytes.size());
}

String PayloadStorage::toAsciiPreview(const uint8_t* data, size_t length) {
    String result;
    for (size_t i = 0; i < length; ++i) {
        const char c = static_cast<char>(data[i]);
        result += (c >= 32 && c <= 126) ? c : '.';
    }
    return result;
}

String PayloadStorage::toAsciiPreview(const std::vector<uint8_t>& bytes) {
    return toAsciiPreview(bytes.data(), bytes.size());
}
