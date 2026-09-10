#pragma once

#include <string>

#include "display_driver.h"
#include "display_types.h"

namespace vipi {

class DisplayRenderer {
public:
    explicit DisplayRenderer(DisplayDriver *driver);

    void render(
        DisplayState state,
        const std::string &status,
        const std::string &transcript,
        const std::string &reply,
        float volume,
        float timer_ratio,
        uint32_t timer_seconds,
        int wifi_rssi,
        bool muted
    );

    void render_volume(float ratio);

private:
    DisplayDriver *driver_;

    void draw_header(int wifi_rssi, bool muted);
    void draw_center_state(DisplayState state);
    void draw_text_page(
        const std::string &title,
        const std::string &text
    );
    void draw_volume(float ratio);
    void draw_timer(float ratio, uint32_t seconds);
};

}  // namespace vipi
