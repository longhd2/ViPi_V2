#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
namespace vipi {
    struct Rgb {
        uint8_t r,g,b;
    };
    enum class LedEffect {
        OFF,
        ROTATING_BLOB,
        THINKING,
        MUTED_SILENT,
        VOLUME_DISPLAY,
        ACTION_TOUCHED,
        TWINKLE,
        ERROR,
        SUCCESS,
        WARNING,
        FAST_PULSE,
        TIMER_RING,
        TIMER_TICK,
        RAINBOW,
        TICK,
        FACTORY_RESET,
        JACK_PLUGGED,
        JACK_UNPLUGGED
    };
    class LedEffects {
        public:
        static int count_from_ratio(float ratio,int n) {
            ratio=std::max(0.f,std::min(1.f,ratio));
            return (int)std::lround(ratio*n);
        }
        static int wrap(int i,int n) {
            return n?((i%n)+n)%n:0;
        }
        void render(LedEffect fx,std::vector<Rgb>&p,uint32_t now,float level=0,int dir=1) {
            int n=(int)p.size();
            for(auto &x:p)x= {
                0,0,0
            };
            if(!n||fx==LedEffect::OFF)return;
            if(fx==LedEffect::VOLUME_DISPLAY||fx==LedEffect::TIMER_RING) {
                int a=count_from_ratio(level,n);
                for(int i=0;i<a;i++)p[i]= {
                    0,120,255
                };
                return;
            }
            if(fx==LedEffect::ROTATING_BLOB) {
                int c=wrap((int)(now/80)*dir,n),width=std::max(1,n/6);
                for(int d=-width;d<=width;d++) {
                    int i=wrap(c+d,n);
                    float k=1.f-(float)std::abs(d)/(width+1);
                    p[i]= {
                        (uint8_t)(20*k),(uint8_t)(180*k),(uint8_t)(255*k)
                    };
                }
                return;
            }
            if(fx==LedEffect::THINKING) {
                int q=std::max(1,n/4),step=(now/120)%n;
                for(int k=0;k<4;k++)p[wrap(step+k*q,n)]= {
                    160,20,255
                };
                return;
            }
            if(fx==LedEffect::MUTED_SILENT) {
                for(auto &x:p)x= {
                    180,0,0
                };
                return;
            }
            if(fx==LedEffect::ERROR) {
                if((now/180)%2==0)for(auto &x:p)x= {
                    255,0,0
                };
                return;
            }
            if(fx==LedEffect::WARNING) {
                if((now/250)%2==0)for(auto &x:p)x= {
                    255,120,0
                };
                return;
            }
            if(fx==LedEffect::SUCCESS) {
                for(auto &x:p)x= {
                    0,180,40
                };
                return;
            }
            // Named slots retained for Satellite1-style effects;
            // Refine independently here.
            for(auto &x:p)x= {
                10,10,10
            };
        }
    };
}
