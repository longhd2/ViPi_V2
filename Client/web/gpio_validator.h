#pragma once
#include "../core/config.h"
#include <map>
#include <string>
namespace vipi {
    inline bool validate_gpio_map(const Config&c,std::string&err) {
        std::map<int,std::string>u;
        auto add=[&](int p,const std::string&n) {
            if(p<0)return true;
            if(p>48) {
                err="invalid "+n;
                return false;
            }
            if(u.count(p)) {
                err="conflict "+u[p]+" / "+n;
                return false;
            }
            u[p]=n;
            return true;
        };
        if (!add(c.mic_ws, "mic_ws") ||
            !add(c.mic_bclk, "mic_bclk") ||
            !add(c.mic_din, "mic_din") ||
            !add(c.spk_bclk, "spk_bclk") ||
            !add(c.spk_ws, "spk_ws") ||
            !add(c.spk_dout, "spk_dout") ||
            !add(c.ptt, "ptt") ||
            !add(c.led.pin, "led")) {
            return false;
        }
        for(int i=0;i<4;i++)if(!add(c.touch[i].pin,"touch"+std::to_string(i+1)))return false;
        return true;
    }
}
