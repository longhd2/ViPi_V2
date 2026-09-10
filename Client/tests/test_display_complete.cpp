#include "../display/display_manager.h"

#include <cassert>

int main() {
    vipi::DisplayConfig cfg;
    cfg.enabled = true;
    cfg.type = vipi::DisplayType::SSD1306;
    cfg.width = 128;
    cfg.height = 64;
    cfg.oled_sda = 41;
    cfg.oled_scl = 42;

    vipi::DisplayManager display;

    assert(display.begin(cfg));

    display.set_state(vipi::DisplayState::LISTENING);
    display.set_status_text("Online");
    display.set_transcript("bat den");
    display.set_reply("Da bat den");
    display.set_wifi_strength(-55);
    display.set_muted(false);
    display.set_timer(0.5f, 90);
    display.show_volume(0.7f);
    display.update(1000);
    display.update(3100);

    return 0;
}
