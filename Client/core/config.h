#pragma once
#include <array>
#include <string>
namespace vipi {
    struct LedConfig {
        int pin {
            48
        };
        int count {
            1
        };
        float brightness {
            0.6f
        };
        bool reverse {
            false
        };
        int offset {
            0
        };
        std::string order {
            "GRB"
        };
    };
    struct TouchConfig {
        int pin {
            -1
        };
        int threshold {
            1800
        };
        int debounce_ms {
            80
        };
        int long_press_ms {
            700
        };
        int repeat_ms {
            120
        };
        std::string short_action {
            "none"
        };
        std::string long_action {
            "none"
        };
    };
    struct AudioConfig {
        int volume {
            70
        };
    };
    struct Config {
        int mic_ws {
            4
        },mic_bclk {
            5
        },mic_din {
            6
        };
        int spk_bclk {
            15
        },spk_ws {
            16
        },spk_dout {
            7
        };
        int ptt {
            39
        };
        LedConfig led {
        };
        std::array<TouchConfig,4> touch {
        };
        AudioConfig audio {
        };
    };
}
