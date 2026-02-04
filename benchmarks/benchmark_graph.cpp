#include "event_bus/event_bus.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <fstream>
#include <iomanip>

using namespace event_bus;

// -----------------------------
// Configurations
// -----------------------------
static constexpr size_t EVENTS_PER_PRODUCER = 500'000;
static constexpr size_t QUEUE_CAPACITY = 1024;
static const std::vector<size_t> PRODUCERS_LIST = {1, 2, 4, 8};
static const std::vector<size_t> CONSUMERS_LIST = {1, 2, 4, 8};

// -----------------------------
// Mutex + queue
// -----------------------------
class MutexQueue {
public:
    MutexQueue(size_t capacity) : capacity_(capacity) {}

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

private:
    std::queue<Event> q_;
    std::mutex mtx_;
    size_t capacity_;
};

// -----------------------------
// EventBus adapter
// -----------------------------
struct EventBusAdapter {
    EventBusAdapter(EventBus& bus) : bus_(bus) {}
    bool push(const Event& ev) { return bus_.try_publish(ev); }
    bool pop(Event& ev) { return bus_.try_consume(ev); }
private:
    EventBus& bus_;
};

// -----------------------------
// Benchmark template
// -----------------------------
template<typename QueueType>
uint64_t run_benchmark(QueueType& q, size_t producers, size_t consumers) {
    std::atomic<uint64_t> produced{0};
    std::atomic<uint64_t> consumed{0};
    std::atomic<bool> start{false};

    // Producers
    std::vector<std::thread> producer_threads;
    for (size_t p = 0; p < producers; ++p) {
        producer_threads.emplace_back([&] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();

            for (size_t i = 0; i < EVENTS_PER_PRODUCER; ++i) {
                Event ev{};
                ev.type = static_cast<uint32_t>(p);
                while (!q.push(ev)) std::this_thread::yield();
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Consumers
    std::vector<std::thread> consumer_threads;
    for (size_t c = 0; c < consumers; ++c) {
        consumer_threads.emplace_back([&] {
            Event ev{};
            while (consumed.load(std::memory_order_relaxed) <
                   producers * EVENTS_PER_PRODUCER) {
                if (q.pop(ev))
                    consumed.fetch_add(1, std::memory_order_relaxed);
                else
                    std::this_thread::yield();
            }
        });
    }

    // Start test
    auto t1 = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);

    for (auto& t : producer_threads) t.join();
    for (auto& t : consumer_threads) t.join();
    auto t2 = std::chrono::steady_clock::now();

    auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    return elapsed_us;
}

// -----------------------------
// CSV Output helper
// -----------------------------
void write_csv_header(std::ofstream& ofs) {
    ofs << "Producers,Consumers,MutexQueue_us,EventBus_us,Speedup\n";
}

void write_csv_row(std::ofstream& ofs, size_t producers, size_t consumers,
                   uint64_t mutex_us, uint64_t bus_us) {
    double speedup = static_cast<double>(mutex_us) / bus_us;
    ofs << producers << "," << consumers << "," << mutex_us << "," << bus_us
        << "," << std::fixed << std::setprecision(2) << speedup << "\n";
}

// -----------------------------
// Main
// -----------------------------
int main() {
    std::ofstream csv("benchmark_results.csv");
    write_csv_header(csv);

    for (size_t p : PRODUCERS_LIST) {
        for (size_t c : CONSUMERS_LIST) {
            // MutexQueue
            MutexQueue mutex_q(QUEUE_CAPACITY);
            uint64_t mutex_time = run_benchmark(mutex_q, p, c);

            // EventBus
            EventBus bus(QUEUE_CAPACITY);
            EventBusAdapter bus_adapter(bus);
            uint64_t bus_time = run_benchmark(bus_adapter, p, c);

            write_csv_row(csv, p, c, mutex_time, bus_time);

            std::cout << "[Run] P=" << p << " C=" << c
                      << " | MutexQueue=" << mutex_time / 1000.0 << " ms"
                      << " | EventBus=" << bus_time / 1000.0 << " ms"
                      << " | Speedup=" << std::fixed
                      << std::setprecision(2)
                      << static_cast<double>(mutex_time)/bus_time
                      << "x\n";
        }
    }

    csv.close();
    std::cout << "\nBenchmark complete. Results saved to 'benchmark_results.csv'.\n";
    return 0;
}
