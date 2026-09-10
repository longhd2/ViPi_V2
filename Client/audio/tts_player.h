#pragma once
#include "audio_manager.h"
namespace vipi {
    class TtsPlayer {
        public:void begin(AudioManager *a) {
            audio_=a;
        }
        bool start() {
            return audio_&&audio_->request(AudioSource::TTS);
        }
        void end() {
            if(audio_)audio_->release(AudioSource::TTS);
        }
        private:AudioManager *audio_ {
            nullptr
        };
    };
}
