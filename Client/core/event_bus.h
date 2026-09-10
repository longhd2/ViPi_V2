#pragma once
#include "types.h"
#include <functional>
#include <vector>
namespace vipi {
    class EventBus {
        public:using Handler=std::function<void(const Event&)>;
        void subscribe(Handler h) {
            handlers_.push_back(h);
        }
        void emit(EventType t,int v=0) {
            Event e {
                t,v
            };
            for(auto &h:handlers_)h(e);
        }
        private:std::vector<Handler> handlers_;
    };
}
