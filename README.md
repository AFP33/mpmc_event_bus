## Introduction
In real-systems (logging, telemetry, message pipelines, metrics, trading systems), many threads produce events and one or more consume them. Mutex-based queues cause:
- lock contention
- priority inversion
- cache-line bouncing
- latency spikes (p99+)

This project shows how to design something better. This project is a **high-performance event bus** that allows multiple producers (threads or components) to publish events and multiple consumers to receive them with minimal latency and minimal synchronization overhead. It is **in-process** (not networked).
Think of it as a faster alternative to `std::queue + std::mutex + std::condition_variable` but designed for **low latency**, **high throughput**, **and predictable behavior**.

## High-Level Architecture 
- Events are written to a fixed-size ring buffer
- Producers claim slots using atomics
- Consumers read slots in order
- No global mutex on hot path
```
┌──────────────┐
│ Producer N   │
└──────┬───────┘
       │
       ▼
┌──────────────────────────────┐
│ Lock-Free / Low-Lock Ring    │
│ Buffer (Event Bus Core)      │
└──────┬───────────────────────┘
       │
       ▼
┌─────────────┐
│ Consumer M  │
└─────────────┘

```
### Design Goals
1. Low latency (especially tail latency)
2. High throughput
3. Predictable performance
3. Minimal memory allocation
4. Clear ownership & lifetime rules
5. No “clever” undefined behavior

d
