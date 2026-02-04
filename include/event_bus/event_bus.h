#ifndef __EVENT_BUS_H__
#define __EVENT_BUS_H__

#include "slot.h"
#include <cstddef>

namespace event_bus {

    class EventBus 
    {
    public:
        explicit EventBus(size_t capacity);
        ~EventBus();

        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        bool try_publish(const Event& event) noexcept;
        bool try_consume(Event& out) noexcept;

        size_t capacity() const noexcept { return capacity_; }

    private:
        struct Slot {
            std::atomic<uint64_t> sequence;
            Event event;

            explicit Slot(uint64_t seq) noexcept
                : sequence(seq), event{} {}

            Slot(const Slot&) = delete;
            Slot& operator=(const Slot&) = delete;
        };

        const size_t capacity_;
        const size_t mask_;

        Slot* buffer_;

        alignas(64) std::atomic<uint64_t> head_;
        alignas(64) std::atomic<uint64_t> tail_;
    };

} // namespace event_bus
#endif // !__EVENT_BUS_H__
