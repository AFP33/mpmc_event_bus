#include "event_bus/event_bus.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using namespace event_bus;

// -----------------------------
// Config
// -----------------------------
static constexpr size_t QUEUE_CAPACITY = 1024;
static constexpr size_t PRODUCERS = 4;
static constexpr size_t CONSUMERS = 4;
static constexpr size_t EVENTS_PER_PRODUCER = 500'000;

// -----------------------------
// Mutex + queue implementation
// -----------------------------
class MutexQueue {
public:
    bool push(const Event& ev) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (q_.size() >= capacity_) return false;
        q_.push(ev);
        return true;
    }

    bool pop(Event& out) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (q_.empty()) return false;
        out = q_.front();
        q_.pop();
        return true;
    }

    explicit MutexQueue(size_t capacity) : capacity_(capacity) {}

private:
    std::queue<Event> q_;
    std::mutex mtx_;
    const size_t capacity_;
};

// -----------------------------
// Benchmark helper
// -----------------------------
template <typename QueueType>
uint64_t run_benchmark(QueueType& q) {
    std::atomic<uint64_t> produced{0};
    std::atomic<uint64_t> consumed{0};
    std::atomic<bool> start{false};

    // Producers
    std::vector<std::thread> producers;
    for (size_t p = 0; p < PRODUCERS; ++p) {
        producers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();

            for (size_t i = 0; i < EVENTS_PER_PRODUCER; ++i) {
                Event ev{};
                ev.type = static_cast<uint32_t>(p);
                while (!q.push(ev)) {
                    std::this_thread::yield();
                }
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Consumers
    std::vector<std::thread> consumers;
    for (size_t c = 0; c < CONSUMERS; ++c) {
        consumers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();

            Event ev{};
            while (consumed.load(std::memory_order_relaxed) <
                   PRODUCERS * EVENTS_PER_PRODUCER) {
                if (q.pop(ev)) {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    auto end = std::chrono::steady_clock::now();
    auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
            .count();

    return elapsed_us;
}

// -----------------------------
// Adapter for EventBus
// -----------------------------
struct EventBusAdapter {
    EventBusAdapter(EventBus& bus) : bus_(bus) {}

    bool push(const Event& ev) { return bus_.try_publish(ev); }
    bool pop(Event& ev) { return bus_.try_consume(ev); }

private:
    EventBus& bus_;
};

// -----------------------------
// Main
// -----------------------------
int main() {
    // 1) Mutex queue
    MutexQueue mutex_q(QUEUE_CAPACITY);
    auto mutex_time = run_benchmark(mutex_q);
    std::cout << "MutexQueue elapsed: " << mutex_time / 1000.0
              << " ms\n";

    // 2) EventBus
    EventBus bus(QUEUE_CAPACITY);
    EventBusAdapter bus_adapter(bus);
    auto bus_time = run_benchmark(bus_adapter);
    std::cout << "EventBus MPMC elapsed: " << bus_time / 1000.0
              << " ms\n";

    double speedup = static_cast<double>(mutex_time) / bus_time;
    std::cout << "Speedup: " << speedup << "x\n";

    return 0;
}
