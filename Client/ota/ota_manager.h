#pragma once
#include "../core/event_bus.h"
#include "../core/state_manager.h"
namespace vipi {
    class OtaManager {
        public:void begin(EventBus*b,StateManager*s) {
            bus_=b;
            state_=s;
        }
        void start() {
            state_->set(DeviceState::OTA);
            bus_->emit(EventType::OTA_START);
        }
        void finish() {
            state_->set(DeviceState::IDLE);
            bus_->emit(EventType::OTA_END);
        }
        private:EventBus*bus_ {
            nullptr
        };
        StateManager*state_ {
            nullptr
        };
    };
}
