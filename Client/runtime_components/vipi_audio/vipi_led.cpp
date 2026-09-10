#include "vipi_led.h"
#include "esp_timer.h"
namespace esphome { namespace vipi_audio {
void ViPiLed::setup(int pin){pin_=pin;gpio_config_t c{};c.pin_bit_mask=1ULL<<pin_;c.mode=GPIO_MODE_OUTPUT;gpio_config(&c);gpio_set_level((gpio_num_t)pin_,0);}
void ViPiLed::set_state(ViPiState s){state_=s;last_=0;}
void ViPiLed::tick(){if(pin_<0)return;uint32_t now=(uint32_t)(esp_timer_get_time()/1000ULL);uint32_t period=0;switch(state_){case ViPiState::BOOTING:period=120;break;case ViPiState::WAKE:period=70;break;case ViPiState::LISTENING:period=300;break;case ViPiState::THINKING:period=180;break;case ViPiState::SPEAKING:period=120;break;case ViPiState::MEDIA:period=700;break;case ViPiState::ERROR:period=100;break;case ViPiState::OTA:period=80;break;case ViPiState::MUTED:gpio_set_level((gpio_num_t)pin_,1);return;default:gpio_set_level((gpio_num_t)pin_,0);return;}if(now-last_>=period){last_=now;level_=!level_;gpio_set_level((gpio_num_t)pin_,level_);}}
}}