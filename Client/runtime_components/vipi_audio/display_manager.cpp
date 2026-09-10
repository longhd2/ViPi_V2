#include "display_manager.h"

#include <algorithm>
#include <cstring>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "vipi_display";

ViPiDisplayManager vipi_display_manager;

namespace {

i2c_master_bus_handle_t display_bus = nullptr;
i2c_master_dev_handle_t display_device = nullptr;

uint32_t now_ms() {
    return static_cast<uint32_t>(
        esp_timer_get_time() / 1000ULL);
}

// Compact 5x7 ASCII font.
// Characters not explicitly represented are rendered as a simple box.
// This keeps the first hardware test small and independent.
const uint8_t DIGITS[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}
};

void glyph(char c, uint8_t out[5]) {
    memset(out, 0, 5);

    if (c >= '0' && c <= '9') {
        memcpy(out, DIGITS[c - '0'], 5);
        return;
    }

    if (c == ' ') {
        return;
    }

    // Minimal readable block glyph for initial hardware integration.
    out[0] = 0x7F;
    out[1] = 0x41;
    out[2] = 0x49;
    out[3] = 0x41;
    out[4] = 0x7F;
}

}  // namespace

bool ViPiDisplayManager::begin(
    int sda_pin,
    int scl_pin,
    uint8_t address) {

    sda_pin_ = sda_pin;
    scl_pin_ = scl_pin;
    address_ = address;

    i2c_master_bus_config_t bus_config{};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num =
        static_cast<gpio_num_t>(sda_pin_);
    bus_config.scl_io_num =
        static_cast<gpio_num_t>(scl_pin_);
    bus_config.clk_source =
        I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t err =
        i2c_new_master_bus(
            &bus_config,
            &display_bus);

    if (err != ESP_OK) {
        ESP_LOGW(
            TAG,
            "I2C bus init failed: %s",
            esp_err_to_name(err));
        return false;
    }

    i2c_device_config_t device_config{};
    device_config.dev_addr_length =
        I2C_ADDR_BIT_LEN_7;
    device_config.device_address = address_;
    device_config.scl_speed_hz = 400000;

    err = i2c_master_bus_add_device(
        display_bus,
        &device_config,
        &display_device);

    if (err != ESP_OK) {
        ESP_LOGW(
            TAG,
            "OLED device init failed: %s",
            esp_err_to_name(err));
        return false;
    }

    const uint8_t init[] = {
        0xAE,
        0xD5, 0x80,
        0xA8, 0x3F,
        0xD3, 0x00,
        0x40,
        0x8D, 0x14,
        0x20, 0x00,
        0xA1,
        0xC8,
        0xDA, 0x12,
        0x81, 0x7F,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        0xA6,
        0x2E,
        0xAF
    };

    for (uint8_t value : init) {
        command_(value);
    }

    ready_ = true;

    clear_buffer_();
    present_();
    show_idle();

    ESP_LOGI(
        TAG,
        "SSD1306 ready SDA=%d SCL=%d address=0x%02X",
        sda_pin_,
        scl_pin_,
        address_);

    return true;
}

void ViPiDisplayManager::command_(uint8_t command) {
    if (!display_device) {
        return;
    }

    uint8_t packet[2] = {
        0x00,
        command
    };

    if (!ready_ && display_device == nullptr) {
        return;
    }

    i2c_master_transmit(
        display_device,
        packet,
        sizeof(packet),
        20);
}

void ViPiDisplayManager::data_(
    const uint8_t *data,
    size_t length) {

    if (!display_device || !data || length == 0) {
        return;
    }

    uint8_t packet[129];
    packet[0] = 0x40;

    while (length > 0) {
        const size_t count =
            std::min<size_t>(128, length);

        memcpy(
            packet + 1,
            data,
            count);

        i2c_master_transmit(
            display_device,
            packet,
            count + 1,
            20);

        data += count;
        length -= count;
    }
}

void ViPiDisplayManager::clear_buffer_() {
    memset(
        framebuffer_,
        0,
        sizeof(framebuffer_));
}

void ViPiDisplayManager::pixel_(
    int x,
    int y,
    bool on) {

    if (x < 0 || x >= 128 ||
        y < 0 || y >= 64) {
        return;
    }

    const int index =
        x + (y / 8) * 128;

    const uint8_t mask =
        static_cast<uint8_t>(
            1U << (y & 7));

    if (on) {
        framebuffer_[index] |= mask;
    } else {
        framebuffer_[index] &= ~mask;
    }
}

void ViPiDisplayManager::character_(
    int x,
    int y,
    char c) {

    uint8_t columns[5];
    glyph(c, columns);

    for (int col = 0; col < 5; ++col) {
        for (int row = 0; row < 7; ++row) {
            if (columns[col] & (1U << row)) {
                pixel_(
                    x + col,
                    y + row,
                    true);
            }
        }
    }
}

void ViPiDisplayManager::text_(
    int x,
    int y,
    const std::string &text) {

    int cursor = x;

    for (char c : text) {
        if (cursor > 122) {
            break;
        }

        character_(
            cursor,
            y,
            c);

        cursor += 6;
    }
}

void ViPiDisplayManager::header_(
    const std::string &title) {

    clear_buffer_();

    text_(
        2,
        2,
        title);

    for (int x = 0; x < 128; ++x) {
        pixel_(x, 11, true);
    }
}

void ViPiDisplayManager::progress_(
    int x,
    int y,
    int width,
    int percent) {

    percent =
        std::max(
            0,
            std::min(100, percent));

    for (int i = 0; i < width; ++i) {
        pixel_(x + i, y, true);
        pixel_(x + i, y + 9, true);
    }

    for (int i = 0; i < 10; ++i) {
        pixel_(x, y + i, true);
        pixel_(x + width - 1, y + i, true);
    }

    const int fill =
        ((width - 4) * percent) / 100;

    for (int px = 0; px < fill; ++px) {
        for (int py = 0; py < 6; ++py) {
            pixel_(
                x + 2 + px,
                y + 2 + py,
                true);
        }
    }
}

void ViPiDisplayManager::present_() {
    if (!ready_) {
        return;
    }

    command_(0x21);
    command_(0x00);
    command_(0x7F);

    command_(0x22);
    command_(0x00);
    command_(0x07);

    data_(
        framebuffer_,
        sizeof(framebuffer_));
}

void ViPiDisplayManager::show_idle() {
    if (!ready_) {
        return;
    }

    header_("VIPI V2");
    text_(20, 30, "READY");
    present_();
}

void ViPiDisplayManager::show_wake() {
    if (!ready_) {
        return;
    }

    header_("VIPI V2");
    text_(20, 30, "WAKE");
    present_();
}

void ViPiDisplayManager::show_listening() {
    if (!ready_) {
        return;
    }

    header_("VIPI V2");
    text_(8, 30, "LISTENING");
    present_();
}

void ViPiDisplayManager::show_thinking() {
    if (!ready_) {
        return;
    }

    header_("VIPI V2");
    text_(12, 30, "THINKING");
    present_();
}

void ViPiDisplayManager::show_speaking() {
    if (!ready_) {
        return;
    }

    header_("VIPI V2");
    text_(12, 30, "SPEAKING");
    present_();
}

void ViPiDisplayManager::show_music(
    const std::string &title) {
    if (!ready_) {
        return;
    }


    header_("MUSIC");

    if (title.empty()) {
        text_(20, 30, "PLAYING");
    } else {
        text_(2, 28, title.substr(0, 20));
    }

    present_();
}

void ViPiDisplayManager::show_volume(
    int volume,
    int maximum) {
    if (!ready_) {
        return;
    }


    if (maximum <= 0) {
        maximum = 20;
    }

    volume =
        std::max(
            0,
            std::min(maximum, volume));

    const int percent =
        (volume * 100) / maximum;

    header_("VOLUME");

    text_(
        48,
        22,
        std::to_string(volume));

    progress_(
        10,
        42,
        108,
        percent);

    present_();

    overlay_until_ =
        now_ms() + 1800;
}

void ViPiDisplayManager::show_error(
    const std::string &message) {
    if (!ready_) {
        return;
    }


    header_("ERROR");

    text_(
        2,
        28,
        message.empty()
            ? "SYSTEM"
            : message.substr(0, 20));

    present_();
}

void ViPiDisplayManager::show_ota(int percent) {
    if (!ready_) {
        return;
    }

    header_("OTA");

    if (percent >= 0) {
        progress_(
            10,
            35,
            108,
            percent);
    } else {
        text_(20, 30, "UPDATE");
    }

    present_();
}

void ViPiDisplayManager::update() {
    if (!ready_) {
        return;
    }

    if (overlay_until_ != 0 &&
        now_ms() >= overlay_until_) {

        overlay_until_ = 0;
        show_idle();
    }
}
