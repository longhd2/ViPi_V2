#pragma once
#include <cstdint>
namespace vipi {
    enum class DeviceState:uint8_t {
        BOOTING,CONNECTING,IDLE,WAKE,LISTENING,THINKING,SPEAKING,MUSIC,MUTED,TIMER,WARNING,ERROR,OTA
    };
    enum class EventType:uint8_t {
        NONE,
        ACTION,
        VOLUME_UP,
        VOLUME_DOWN,
        MUTE_TOGGLE,
        WAKE_DETECTED,
        TTS_START,
        TTS_END,
        MEDIA_START,
        MEDIA_STOP,
        CONNECTED,
        DISCONNECTED,
        WARNING,
        ERROR,
        OTA_START,
        OTA_END
    };
    enum class AudioSource:uint8_t {
        NONE=0,MEDIA=10,TTS=20,SYSTEM=30,ALARM=40
    };
    struct Event {
        EventType type {
            EventType::NONE
        };
        int value {
            0
        };
    };
}
