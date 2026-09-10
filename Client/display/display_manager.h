#pragma once

#include <cstdint>
#include <string>

class ViPiDisplayManager {
 public:
    bool begin(
        int sda_pin = 41,
        int scl_pin = 42,
        uint8_t address = 0x3C);

    void update();

    void show_idle();
    void show_wake();
    void show_listening();
    void show_thinking();
    void show_speaking();
    void show_music(const std::string &title = "");
    void show_volume(int volume, int maximum = 20);
    void show_error(const std::string &message = "");
    void show_ota(int percent = -1);

 private:
    void command_(uint8_t command);
    void data_(const uint8_t *data, size_t length);

    void clear_buffer_();
    void pixel_(int x, int y, bool on);
    void text_(int x, int y, const std::string &text);
    void character_(int x, int y, char c);
    void present_();
    void header_(const std::string &title);
    void progress_(int x, int y, int width, int percent);

    int sda_pin_{41};
    int scl_pin_{42};
    uint8_t address_{0x3C};

    uint8_t framebuffer_[1024]{};

    bool ready_{false};
    uint32_t overlay_until_{0};
};

extern ViPiDisplayManager vipi_display_manager;
