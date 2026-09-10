#include <cassert>
#include <iostream>
#include "../core/config.h"
#include "../core/event_bus.h"
#include "../core/state_manager.h"
#include "../audio/audio_manager.h"
#include "../audio/tts_player.h"
#include "../audio/media_player.h"
#include "../led/led_manager.h"
#include "../touch/touch_manager.h"
#include "../web/gpio_validator.h"
using namespace vipi;
int main() {
    Config c;
    c.led.count=24;
    c.touch[0].short_action="voice";
    c.touch[1].short_action="volume_down";
    c.touch[2].short_action="volume_up";
    c.touch[3].short_action="mute";
    EventBus bus;
    StateManager st;
    st.set(DeviceState::IDLE);
    AudioManager audio;
    audio.begin(&c,&bus,&st);
    assert(audio.volume()==70);
    bus.emit(EventType::VOLUME_UP);
    assert(audio.volume()==75);
    bus.emit(EventType::VOLUME_DOWN);
    assert(audio.volume()==70);
    MediaPlayer media;
    media.begin(&audio);
    TtsPlayer tts;
    tts.begin(&audio);
    assert(media.start());
    assert(st.get()==DeviceState::MUSIC);
    assert(tts.start());
    assert(st.get()==DeviceState::SPEAKING);
    tts.end();
    assert(st.get()==DeviceState::IDLE);
    assert(AudioManager::apply_gain(10000,50)==5000);
    assert(AudioManager::apply_gain(30000,100)==30000);
    LedManager led;
    led.begin(&c,&st);
    led.set_volume_preview(50);
    led.render(0);
    int lit=0;
    for(auto&p:led.pixels())if(p.r||p.g||p.b)lit++;
    assert(lit==12);
    led.clear_preview();
    st.set(DeviceState::THINKING);
    led.render(120);
    assert(led.pixels().size()==24);
    TouchManager touch;
    touch.begin(&c,&bus);
    int actions=0;
    bus.subscribe([&](const Event&e){if(e.type==EventType::ACTION)actions++;});
    touch.update_key(0,true,100);
    touch.update_key(0,false,200);
    assert(actions==1);
    std::string err;
    assert(validate_gpio_map(c,err));
    c.touch[0].pin=48;
    assert(!validate_gpio_map(c,err));
    bus.emit(EventType::MUTE_TOGGLE);
    assert(audio.mic_muted());
    std::cout<<"CLIENT LOGIC TESTS: PASS\n";
    return 0;
}
