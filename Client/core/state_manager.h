#pragma once
#include "types.h"
namespace vipi {
    class StateManager {
        public:void set(DeviceState s) {
            state_=s;
        }
        DeviceState get()const {
            return state_;
        }
        private:DeviceState state_ {
            DeviceState::BOOTING
        };
    };
}
