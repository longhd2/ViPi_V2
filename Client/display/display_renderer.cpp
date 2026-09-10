#include "display_renderer.h"

#include <cstdio>

#include "display_icons.h"

namespace vipi {

DisplayRenderer::DisplayRenderer(DisplayDriver *driver)
    : driver_(driver) {
}

void DisplayRenderer::render(
    DisplayState state,
    const std::string &status,
    const std::string &transcript,
    const std::string &reply,
    float volume,
    float timer_ratio,
    uint32_t timer_seconds,
    int wifi_rssi,
    bool muted
) {
    if (driver_ == nullptr) {
        return;
    }

    driver_->clear();
    draw_header(wifi_rssi, muted);

    switch (state) {
        case DisplayState::LISTENING:
            driver_->draw_icon(56, 18, icons::MIC, 16, 16);
            driver_->draw_text(64, 42, "Dang nghe...", 1, true);
            break;

        case DisplayState::THINKING:
            driver_->draw_text(64, 20, "...", 2, true);
            driver_->draw_text(64, 46, "Dang xu ly", 1, true);
            break;

        case DisplayState::SPEAKING:
            driver_->draw_icon(56, 16, icons::SPEAKER, 16, 16);
            draw_text_page("ViPi", reply);
            break;

        case DisplayState::MUSIC:
            driver_->draw_icon(56, 16, icons::MUSIC, 16, 16);
            driver_->draw_text(64, 42, "Dang phat nhac", 1, true);
            break;

        case DisplayState::TIMER:
        case DisplayState::ALARM:
            draw_timer(timer_ratio, timer_seconds);
            break;

        case DisplayState::MUTED:
            driver_->draw_icon(56, 18, icons::MUTED, 16, 16);
            driver_->draw_text(64, 42, "Mic tat", 1, true);
            break;

        case DisplayState::WARNING:
        case DisplayState::ERROR:
            driver_->draw_icon(56, 14, icons::WARNING, 16, 16);
            driver_->draw_text(64, 40, status, 1, true);
            break;

        case DisplayState::WAKE:
            driver_->draw_text(64, 18, "ViPi", 2, true);
            driver_->draw_text(64, 45, "Xin chao!", 1, true);
            break;

        case DisplayState::CONNECTING:
            driver_->draw_text(64, 22, "ViPi", 2, true);
            driver_->draw_text(64, 48, "Dang ket noi", 1, true);
            break;

        case DisplayState::OTA:
            driver_->draw_text(64, 20, "OTA", 2, true);
            driver_->draw_text(64, 46, status, 1, true);
            break;

        case DisplayState::BOOTING:
            driver_->draw_text(64, 20, "ViPi", 2, true);
            driver_->draw_text(64, 46, "Khoi dong...", 1, true);
            break;

        case DisplayState::IDLE:
        default:
            if (!transcript.empty()) {
                draw_text_page("Ban", transcript);
            } else {
                driver_->draw_text(64, 24, "ViPi", 2, true);
                driver_->draw_text(64, 50, status, 1, true);
            }
            break;
    }

    // Volume is intentionally an overlay when a caller sets it.
    (void)volume;

    driver_->present();
}


void DisplayRenderer::render_volume(float ratio) {
    if (driver_ == nullptr) {
        return;
    }

    driver_->clear();
    draw_volume(ratio);
    driver_->present();
}

void DisplayRenderer::draw_header(int wifi_rssi, bool muted) {
    if (wifi_rssi > -90) {
        driver_->draw_icon(2, 0, icons::WIFI, 16, 16);
    }

    if (muted) {
        driver_->draw_icon(
            driver_->width() - 18,
            0,
            icons::MUTED,
            16,
            16
        );
    }
}

void DisplayRenderer::draw_center_state(DisplayState state) {
    (void)state;
}

void DisplayRenderer::draw_text_page(
    const std::string &title,
    const std::string &text
) {
    driver_->draw_text(64, 16, title, 1, true);

    std::string short_text = text;

    if (short_text.size() > 36) {
        short_text.resize(36);
        short_text += "...";
    }

    driver_->draw_text(64, 38, short_text, 1, true);
}

void DisplayRenderer::draw_volume(float ratio) {
    driver_->draw_text(64, 20, "Am luong", 1, true);
    driver_->draw_progress(12, 40, 104, 10, ratio);
}

void DisplayRenderer::draw_timer(
    float ratio,
    uint32_t seconds
) {
    char time_text[16];

    const uint32_t minutes = seconds / 60;
    const uint32_t remain = seconds % 60;

    std::snprintf(
        time_text,
        sizeof(time_text),
        "%02lu:%02lu",
        static_cast<unsigned long>(minutes),
        static_cast<unsigned long>(remain)
    );

    driver_->draw_icon(10, 18, icons::TIMER, 16, 16);
    driver_->draw_text(70, 18, time_text, 2, true);
    driver_->draw_progress(12, 48, 104, 8, ratio);
}

}  // namespace vipi
