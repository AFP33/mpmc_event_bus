#include "event_bus/event_bus.h"

#include <cassert>
#include <new>

namespace event_bus {

    static bool is_power_of_two(size_t x) {
        return x && ((x & (x - 1)) == 0);
    }

    EventBus::EventBus(size_t capacity)
        : capacity_(capacity)
        , mask_(capacity - 1)
        , buffer_(nullptr)
        , head_(0)
        , tail_(0)
    {
        assert(is_power_of_two(capacity_));

        buffer_ = static_cast<Slot*>(
            ::operator new[](sizeof(Slot)* capacity_));

        for (size_t i = 0; i < capacity_; ++i) {
            new (&buffer_[i]) Slot(i);
        }
    }

    EventBus::~EventBus() {
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].~Slot();
        }
        ::operator delete[](buffer_);
    }

    bool EventBus::try_publish(const Event& event) noexcept {
        uint64_t pos = tail_.load(std::memory_order_relaxed);

        for (;;) {
            Slot& slot = buffer_[pos & mask_];
            uint64_t seq = slot.sequence.load(std::memory_order_acquire);
            int64_t diff = static_cast<int64_t>(seq) -
                static_cast<int64_t>(pos);

            if (diff == 0) {
                if (tail_.compare_exchange_weak(
                    pos, pos + 1, std::memory_order_relaxed)) {
                    slot.event = event;
                    slot.sequence.store(
                        pos + 1, std::memory_order_release);
                    return true;
                }
            }
            else if (diff < 0) {
                return false;
            }
            else {
                pos = tail_.load(std::memory_order_relaxed);
            }
        }
    }

    bool EventBus::try_consume(Event& out) noexcept {
        uint64_t pos = head_.load(std::memory_order_relaxed);

        for (;;) {
            Slot& slot = buffer_[pos & mask_];
            uint64_t seq = slot.sequence.load(std::memory_order_acquire);
            int64_t diff = static_cast<int64_t>(seq) -
                static_cast<int64_t>(pos + 1);

            if (diff == 0) {
                if (head_.compare_exchange_weak(
                    pos, pos + 1, std::memory_order_relaxed)) {
                    out = slot.event;
                    slot.sequence.store(
                        pos + capacity_, std::memory_order_release);
                    return true;
                }
            }
            else if (diff < 0) {
                return false;
            }
            else {
                pos = head_.load(std::memory_order_relaxed);
            }
        }
    }

} // namespace event_bus
