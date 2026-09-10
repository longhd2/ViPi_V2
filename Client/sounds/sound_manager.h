#pragma once
#include "../audio/audio_manager.h"
namespace vipi {
    enum class SoundId {
        BOOT,CONNECTED,DISCONNECTED,WAKE,LISTENING,SUCCESS,WARNING,ERROR,MUTE,UNMUTE,VOLUME_MIN,VOLUME_MAX,TIMER,ALARM,OTA_START,OTA_SUCCESS
    };
    class SoundManager {
        public:void begin(AudioManager*a) {
            audio_=a;
        }
        bool start_system_sound() {
            return audio_&&audio_->request(AudioSource::SYSTEM);
        }
        void end_system_sound() {
            if(audio_)audio_->release(AudioSource::SYSTEM);
        }
        bool start_alarm() {
            return audio_&&audio_->request(AudioSource::ALARM);
        }
        void end_alarm() {
            if(audio_)audio_->release(AudioSource::ALARM);
        }
        private:AudioManager*audio_ {
            nullptr
        };
    };
}
