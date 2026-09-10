#pragma once
#include <atomic>
namespace esphome { namespace vipi_audio {
enum class ViPiState : unsigned char { BOOTING, IDLE, WAKE, LISTENING, THINKING, SPEAKING, MEDIA, MUTED, ERROR, OTA };
class ViPiStateStore { public: void set(ViPiState s){state_.store(s);} ViPiState get()const{return state_.load();} private: std::atomic<ViPiState> state_{ViPiState::BOOTING}; };
}}