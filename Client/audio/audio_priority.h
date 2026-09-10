#pragma once
#include "../core/types.h"
namespace vipi {
    inline int priority(AudioSource s) {
        return static_cast<int>(s);
    }
    inline bool can_preempt(AudioSource incoming,AudioSource current) {
        return priority(incoming)>priority(current);
    }
}
