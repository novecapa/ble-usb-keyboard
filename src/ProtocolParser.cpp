#include "ProtocolParser.h"

#include "BleSimulatorConfig.h"
#include "PayloadStorage.h"

#include <esp_system.h>
#include <mbedtls/aes.h>

ProtocolParser::ProtocolParser() {
    parseHexKey(SimulatorConfig::kMasterKeyHex, masterKey_);
}

std::vector<uint8_t> ProtocolParser::beginSession() {
    readerRandom_.assign(16, 0);
    phoneRandom_.clear();
    fillRandom16(readerRandom_);
    state_ = SessionState::kWaitingStep1Response;
    return readerRandom_;
}

ProtocolParser::TransitionResult ProtocolParser::handleNotification(const uint8_t* data, size_t length) {
    TransitionResult result;
    result.nextState = state_;

    if (state_ == SessionState::kWaitingStep1Response) {
        if (length < 16) {
            state_ = SessionState::kFailed;
            result.nextState = state_;
            result.message = "step1 notify demasiado corto";
            return result;
        }

        uint8_t expected[16] = {0};
        if (!aes128EcbEncrypt(masterKey_, readerRandom_.data(), expected)) {
            state_ = SessionState::kFailed;
            result.nextState = state_;
            result.message = "error AES local en paso1";
            return result;
        }

        for (size_t i = 0; i < 16; ++i) {
            if (data[i] != expected[i]) {
                state_ = SessionState::kFailed;
                result.nextState = state_;
                result.message = "step1 notify no coincide con AES(rand_twn)";
                return result;
            }
        }

        state_ = SessionState::kWaitingPhoneRandom;
        result.nextState = state_;
        result.shouldWrite = true;
        result.nextWrite = makeStep2Trigger();
        result.message = (length > 16)
                             ? "step1 OK; notify con bytes extra tolerados"
                             : "step1 OK";
        return result;
    }

    if (state_ == SessionState::kWaitingPhoneRandom) {
        if (length < 16) {
            state_ = SessionState::kFailed;
            result.nextState = state_;
            result.message = "step2 notify demasiado corto";
            return result;
        }

        phoneRandom_.assign(data, data + 16);
        std::vector<uint8_t> encrypted(16, 0);
        if (!aes128EcbEncrypt(masterKey_, phoneRandom_.data(), encrypted.data())) {
            state_ = SessionState::kFailed;
            result.nextState = state_;
            result.message = "error AES local en paso3";
            return result;
        }

        state_ = SessionState::kWaitingFinalPayload;
        result.nextState = state_;
        result.shouldWrite = true;
        result.nextWrite = encrypted;
        result.message = "step2 OK; rand_phone recibido";
        return result;
    }

    if (state_ == SessionState::kWaitingFinalPayload) {
        std::vector<uint8_t> payload(data, data + length);
        state_ = SessionState::kAuthenticated;
        result.nextState = state_;
        result.completed = true;
        result.nextWrite = payload;
        result.message = "payload final recibido";
        return result;
    }

    result.message = "notify ignorado por estado actual";
    return result;
}

void ProtocolParser::reset() {
    state_ = SessionState::kIdle;
    readerRandom_.clear();
    phoneRandom_.clear();
}

bool ProtocolParser::selfTest() const {
    static constexpr char kNistKeyHex[] = "2b7e151628aed2a6abf7158809cf4f3c";
    static constexpr char kNistPlaintextHex[] = "6bc1bee22e409f96e93d7e117393172a";
    static constexpr char kNistCiphertextHex[] = "3ad77bb40d7a3660a89ecaf32466ef97";

    std::array<uint8_t, 16> key = {};
    uint8_t plaintext[16] = {0};
    uint8_t expected[16] = {0};
    uint8_t actual[16] = {0};

    if (!parseHexKey(kNistKeyHex, key)) {
        return false;
    }
    if (!parseHexBytes(kNistPlaintextHex, plaintext, sizeof(plaintext))) {
        return false;
    }
    if (!parseHexBytes(kNistCiphertextHex, expected, sizeof(expected))) {
        return false;
    }
    if (!aes128EcbEncrypt(key, plaintext, actual)) {
        return false;
    }

    for (size_t i = 0; i < sizeof(actual); ++i) {
        if (actual[i] != expected[i]) {
            return false;
        }
    }

    return true;
}

ProtocolParser::SessionState ProtocolParser::state() const {
    return state_;
}

const std::vector<uint8_t>& ProtocolParser::readerRandom() const {
    return readerRandom_;
}

const std::vector<uint8_t>& ProtocolParser::phoneRandom() const {
    return phoneRandom_;
}

bool ProtocolParser::aes128EcbEncrypt(const std::array<uint8_t, 16>& key,
                                      const uint8_t* input,
                                      uint8_t* output) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);

    if (mbedtls_aes_setkey_enc(&ctx, key.data(), 128) != 0) {
        mbedtls_aes_free(&ctx);
        return false;
    }

    const int rc = mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, input, output);
    mbedtls_aes_free(&ctx);
    return rc == 0;
}

bool ProtocolParser::parseHexKey(const char* hex, std::array<uint8_t, 16>& outKey) {
    return parseHexBytes(hex, outKey.data(), outKey.size());
}

bool ProtocolParser::parseHexBytes(const char* hex, uint8_t* outBytes, size_t outLen) {
    const size_t expectedLen = outLen * 2;
    if (strlen(hex) != expectedLen) {
        return false;
    }

    auto hexNibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };

    for (size_t i = 0; i < outLen; ++i) {
        const int hi = hexNibble(hex[i * 2]);
        const int lo = hexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        outBytes[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

std::vector<uint8_t> ProtocolParser::makeStep2Trigger() {
    if (!SimulatorConfig::kSendStep2AsSixteenZeroes) {
        return {0x01};
    }
    return std::vector<uint8_t>(16, 0x00);
}

void ProtocolParser::fillRandom16(std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < 16; ++i) {
        bytes[i] = static_cast<uint8_t>(esp_random() & 0xFF);
    }
}
