#include "../display/ssd1306_driver.h"
#include "../display/display_icons.h"

#include <cassert>

int main() {
    vipi::DisplayConfig cfg;
    cfg.enabled = true;
    cfg.type = vipi::DisplayType::SSD1306;
    cfg.width = 128;
    cfg.height = 64;
    cfg.oled_sda = 41;
    cfg.oled_scl = 42;

    vipi::Ssd1306Driver oled;

    assert(oled.begin(cfg));
    assert(oled.width() == 128);
    assert(oled.height() == 64);

    oled.clear();
    oled.draw_text(64, 10, "ViPi", 2, true);
    oled.draw_icon(0, 0, vipi::icons::WIFI, 16, 16);
    oled.draw_progress(10, 50, 108, 8, 0.75f);
    oled.set_brightness(80);
    oled.present();

    return 0;
}
