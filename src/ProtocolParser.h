#pragma once

#include <Arduino.h>
#include <array>
#include <vector>

class ProtocolParser {
  public:
    enum class SessionState {
        kIdle,
        kWaitingStep1Response,
        kWaitingPhoneRandom,
        kWaitingFinalPayload,
        kAuthenticated,
        kFailed
    };

    struct TransitionResult {
        bool shouldWrite = false;
        std::vector<uint8_t> nextWrite;
        bool completed = false;
        SessionState nextState = SessionState::kIdle;
        const char* message = "";
    };

    ProtocolParser();

    std::vector<uint8_t> beginSession();
    TransitionResult handleNotification(const uint8_t* data, size_t length);
    void reset();
    bool selfTest() const;

    SessionState state() const;
    const std::vector<uint8_t>& readerRandom() const;
    const std::vector<uint8_t>& phoneRandom() const;

  private:
    static bool aes128EcbEncrypt(const std::array<uint8_t, 16>& key,
                                 const uint8_t* input,
                                 uint8_t* output);
    static bool parseHexBytes(const char* hex, uint8_t* outBytes, size_t outLen);
    static bool parseHexKey(const char* hex, std::array<uint8_t, 16>& outKey);
    static std::vector<uint8_t> makeStep2Trigger();
    static void fillRandom16(std::vector<uint8_t>& bytes);

    std::array<uint8_t, 16> masterKey_{};
    std::vector<uint8_t> readerRandom_;
    std::vector<uint8_t> phoneRandom_;
    SessionState state_ = SessionState::kIdle;
};
