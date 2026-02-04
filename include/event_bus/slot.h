#ifndef __SLOT_H__
#define __SLOT_H__

#include "event.h"
#include <atomic>

namespace event_bus {

    struct Slot
    {
        std::atomic<uint64_t> sequence;
        Event event;

        Slot(uint64_t seq) noexcept : sequence(seq), event{} {}

        Slot(const Slot&) = delete;
        Slot& operator=(const Slot&) = delete;
    };
}
#endif // !__SLOT_H__