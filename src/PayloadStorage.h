#pragma once

#include <Arduino.h>
#include <vector>

class PayloadStorage {
  public:
    void recordConnectionEvent(const char* eventLabel);
    void recordStep(const char* stepLabel, const std::vector<uint8_t>& bytes);
    void recordFinalPayload(const std::vector<uint8_t>& bytes);

    const std::vector<uint8_t>& lastPayload() const;
    String lastPayloadHex() const;
    String lastPayloadAscii() const;

    static String toHex(const uint8_t* data, size_t length);
    static String toHex(const std::vector<uint8_t>& bytes);
    static String toAsciiPreview(const uint8_t* data, size_t length);
    static String toAsciiPreview(const std::vector<uint8_t>& bytes);

  private:
    std::vector<uint8_t> lastPayload_;
    uint32_t lastPayloadTimestampMs_ = 0;
};
