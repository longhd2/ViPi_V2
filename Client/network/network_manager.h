#pragma once
#include "../core/event_bus.h"
#include "../core/state_manager.h"
namespace vipi {
    class NetworkManager {
        public:void connected() {
            state_->set(DeviceState::IDLE);
            bus_->emit(EventType::CONNECTED);
        }
        void disconnected() {
            state_->set(DeviceState::WARNING);
            bus_->emit(EventType::DISCONNECTED);
        }
        void begin(EventBus*b,StateManager*s) {
            bus_=b;
            state_=s;
        }
        private:EventBus*bus_ {
            nullptr
        };
        StateManager*state_ {
            nullptr
        };
    };
}
