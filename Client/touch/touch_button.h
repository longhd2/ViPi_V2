#pragma once
#include "../core/config.h"
#include <cstdint>
namespace vipi {
    struct TouchEvents {
        bool short_press {
            false
        };
        bool long_press {
            false
        };
        bool repeat {
            false
        };
    };
    class TouchButton {
        public:void configure(const TouchConfig&c) {
            cfg_=c;
        }
        TouchEvents update(bool touched,uint32_t now) {
            TouchEvents e {
            };
            if(touched&&!down_) {
                down_=true;
                down_at_=last_repeat_=now;
                long_sent_=false;
            }
            else if(touched&&down_) {
                if(!long_sent_&&now-down_at_>=(uint32_t)cfg_.long_press_ms) {
                    e.long_press=true;
                    long_sent_=true;
                    last_repeat_=now;
                }
                else if(long_sent_&&now-last_repeat_>=(uint32_t)cfg_.repeat_ms) {
                    e.repeat=true;
                    last_repeat_=now;
                }
            }
            else if(!touched&&down_) {
                if(!long_sent_&&now-down_at_>=(uint32_t)cfg_.debounce_ms)e.short_press=true;
                down_=false;
            }
            return e;
        }
        private:TouchConfig cfg_ {
        };
        bool down_ {
            false
        },long_sent_ {
            false
        };
        uint32_t down_at_ {
            0
        },last_repeat_ {
            0
        };
    };
}
