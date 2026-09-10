#pragma once

#include <cstdint>
#include "esp_log.h"
#include "esp_heap_caps.h"

enum class ViPiState : uint8_t {
    BOOTING,
    CONNECTING,
    IDLE,
    WAKE,
    LISTENING,
    THINKING,
    SPEAKING,
    MUSIC,
    TIMER,
    ALARM,
    SUCCESS,
    WARNING,
    ERROR,
    OTA
};

class ViPiDiagnostics {
public:
    static const char *state_name(ViPiState state) {
        switch (state) {
            case ViPiState::BOOTING:    return "BOOTING";
            case ViPiState::CONNECTING: return "CONNECTING";
            case ViPiState::IDLE:       return "IDLE";
            case ViPiState::WAKE:       return "WAKE";
            case ViPiState::LISTENING:  return "LISTENING";
            case ViPiState::THINKING:   return "THINKING";
            case ViPiState::SPEAKING:   return "SPEAKING";
            case ViPiState::MUSIC:      return "MUSIC";
            case ViPiState::TIMER:      return "TIMER";
            case ViPiState::ALARM:      return "ALARM";
            case ViPiState::SUCCESS:    return "SUCCESS";
            case ViPiState::WARNING:    return "WARNING";
            case ViPiState::ERROR:      return "ERROR";
            case ViPiState::OTA:        return "OTA";
            default:                    return "UNKNOWN";
        }
    }

    static void state(ViPiState from, ViPiState to) {
        ESP_LOGI(
            "VIPI.STATE",
            "%s -> %s",
            state_name(from),
            state_name(to)
        );
        memory(state_name(to));
    }

    static void module(const char *module, const char *event) {
        ESP_LOGI("VIPI", "[%s] %s", module, event);
    }

    static void error(const char *module, const char *event) {
        ESP_LOGE("VIPI", "[%s] %s", module, event);
        memory("ERROR");
    }

    static void memory(const char *where) {
        const size_t internal_free =
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        const size_t internal_largest =
            heap_caps_get_largest_free_block(
                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
            );

        const size_t internal_min =
            heap_caps_get_minimum_free_size(
                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
            );

        const size_t psram_free =
            heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        const size_t psram_largest =
            heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

        const size_t psram_min =
            heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);

        ESP_LOGI(
            "VIPI.MEM",
            "[%s] INT free=%uKB largest=%uKB min=%uKB | "
            "PSRAM free=%uKB largest=%uKB min=%uKB",
            where,
            (unsigned)(internal_free / 1024),
            (unsigned)(internal_largest / 1024),
            (unsigned)(internal_min / 1024),
            (unsigned)(psram_free / 1024),
            (unsigned)(psram_largest / 1024),
            (unsigned)(psram_min / 1024)
        );
    }
};
