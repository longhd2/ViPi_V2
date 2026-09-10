#pragma once
#include "audio_manager.h"
namespace vipi {
    class MediaPlayer {
        public:void begin(AudioManager *a) {
            audio_=a;
        }
        bool start() {
            return audio_&&audio_->request(AudioSource::MEDIA);
        }
        void stop() {
            if(audio_)audio_->release(AudioSource::MEDIA);
        }
        private:AudioManager *audio_ {
            nullptr
        };
    };
}
