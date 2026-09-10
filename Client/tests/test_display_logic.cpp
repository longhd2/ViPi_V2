#include "../display/display_manager.h"

#include <cassert>

int main() {
    vipi::DisplayConfig config;
    config.enabled = true;
    config.type = vipi::DisplayType::ST7789;
    config.width = 240;
    config.height = 240;

    vipi::DisplayManager display;
    display.begin(config);

    display.set_state(vipi::DisplayState::LISTENING);
    display.set_volume(1.5f);
    display.set_timer(-0.5f, 30);
    display.set_muted(true);
    display.update(1000);

    assert(display.config().enabled);
    assert(display.config().width == 240);
    return 0;
}
