#include "event_bus/event_bus.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <algorithm>
#include <cstring>   // memcpy

using namespace event_bus;
using Clock = std::chrono::steady_clock;

// --------------------------------------------------
// Config
// --------------------------------------------------
static constexpr size_t EVENTS_PER_PRODUCER = 500'000;
static constexpr size_t QUEUE_CAPACITY = 1024;

static const std::vector<size_t> PRODUCERS_LIST = {1, 2, 4, 8};
static const std::vector<size_t> CONSUMERS_LIST = {1, 2, 4, 8};

// --------------------------------------------------
// Benchmark Event
// --------------------------------------------------
struct BenchEvent {
    uint64_t id;
};

// --------------------------------------------------
// MutexQueue
// --------------------------------------------------
class MutexQueue {
public:
    explicit MutexQueue(size_t cap) : capacity_(cap) {}

    bool push(const BenchEvent& ev) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (q_.size() >= capacity_) return false;
        q_.push(ev);
        return true;
    }

    bool pop(BenchEvent& ev) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (q_.empty()) return false;
        ev = q_.front();
        q_.pop();
        return true;
    }

private:
    std::queue<BenchEvent> q_;
    std::mutex mtx_;
    size_t capacity_;
};

// --------------------------------------------------
// EventBus Adapter (FIXED)
// --------------------------------------------------
struct EventBusAdapter {
    explicit EventBusAdapter(EventBus& bus) : bus_(bus) {}

    bool push(const BenchEvent& ev) {
        Event e{};
        e.type = 0;
        std::memcpy(e.payload.data(), &ev.id, sizeof(ev.id));
        return bus_.try_publish(e);
    }

    bool pop(BenchEvent& ev) {
        Event e{};
        if (!bus_.try_consume(e))
            return false;

        std::memcpy(&ev.id, e.payload.data(), sizeof(ev.id));
        return true;
    }

private:
    EventBus& bus_;
};

// --------------------------------------------------
// Benchmark Result
// --------------------------------------------------
struct Result {
    uint64_t elapsed_us;
    double avg_latency_us;
    double p99_latency_us;
};

// --------------------------------------------------
// Benchmark Core
// --------------------------------------------------
template <typename Queue>
Result run_benchmark(Queue& q, size_t producers, size_t consumers) {
    const size_t total_events = producers * EVENTS_PER_PRODUCER;

    std::vector<Clock::time_point> timestamps(total_events);
    std::vector<uint64_t> latencies;
    latencies.reserve(total_events);

    std::atomic<size_t> produced{0};
    std::atomic<size_t> consumed{0};
    std::atomic<bool> start{false};

    std::mutex latency_mtx;

    // Producers
    std::vector<std::thread> prod_threads;
    for (size_t p = 0; p < producers; ++p) {
        prod_threads.emplace_back([&, p] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();

            size_t base = p * EVENTS_PER_PRODUCER;
            for (size_t i = 0; i < EVENTS_PER_PRODUCER; ++i) {
                size_t id = base + i;
                timestamps[id] = Clock::now();

                BenchEvent ev{id};
                while (!q.push(ev))
                    std::this_thread::yield();

                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Consumers
    std::vector<std::thread> cons_threads;
    for (size_t c = 0; c < consumers; ++c) {
        cons_threads.emplace_back([&] {
            BenchEvent ev{};
            while (consumed.load(std::memory_order_relaxed) < total_events) {
                if (q.pop(ev)) {
                    auto now = Clock::now();
                    auto latency =
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            now - timestamps[ev.id]).count();

                    {
                        std::lock_guard<std::mutex> lock(latency_mtx);
                        latencies.push_back(latency);
                    }

                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    auto t1 = Clock::now();
    start.store(true, std::memory_order_release);

    for (auto& t : prod_threads) t.join();
    for (auto& t : cons_threads) t.join();
    auto t2 = Clock::now();

    uint64_t elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

    std::sort(latencies.begin(), latencies.end());

    double avg = 0.0;
    for (auto v : latencies) avg += v;
    avg /= latencies.size();

    size_t p99_index = static_cast<size_t>(latencies.size() * 0.99);
    double p99 = latencies[p99_index];

    return {elapsed_us, avg, p99};
}

// --------------------------------------------------
// CSV Helpers
// --------------------------------------------------
void write_header(std::ofstream& out) {
    out << "Producers,Consumers,"
        << "Mutex_us,MutexAvgLatency_us,MutexP99Latency_us,"
        << "Bus_us,BusAvgLatency_us,BusP99Latency_us\n";
}

// --------------------------------------------------
// Main
// --------------------------------------------------
int main() {
    std::ofstream csv("benchmark_results.csv");
    write_header(csv);

    for (auto p : PRODUCERS_LIST) {
        for (auto c : CONSUMERS_LIST) {

            MutexQueue mq(QUEUE_CAPACITY);
            auto r_mutex = run_benchmark(mq, p, c);

            EventBus bus(QUEUE_CAPACITY);
            EventBusAdapter adapter(bus);
            auto r_bus = run_benchmark(adapter, p, c);

            csv << p << "," << c << ","
                << r_mutex.elapsed_us << ","
                << r_mutex.avg_latency_us << ","
                << r_mutex.p99_latency_us << ","
                << r_bus.elapsed_us << ","
                << r_bus.avg_latency_us << ","
                << r_bus.p99_latency_us << "\n";

            std::cout
                << "[P=" << p << " C=" << c << "] "
                << "Mutex: " << r_mutex.elapsed_us / 1000.0 << " ms | "
                << "Bus: " << r_bus.elapsed_us / 1000.0 << " ms\n";
        }
    }

    std::cout << "\nBenchmark complete. CSV written.\n";
    return 0;
}
