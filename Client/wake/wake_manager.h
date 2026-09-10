#pragma once
#include "../core/event_bus.h"
#include "../core/state_manager.h"
namespace vipi {
    class WakeManager {
        public:void begin(EventBus*b,StateManager*s) {
            bus_=b;
            state_=s;
        }
        void detected() {
            state_->set(DeviceState::WAKE);
            bus_->emit(EventType::WAKE_DETECTED);
        }
        private:EventBus*bus_ {
            nullptr
        };
        StateManager*state_ {
            nullptr
        };
    };
}
