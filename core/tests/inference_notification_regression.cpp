// Regression tests for the bounded SPSC queue and atomic notification.
// Uses explicit checks so Release builds exercise every test.
#include <guitarmidi/spscringbuffer.hpp>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

using GuitarMidi::SpscRingBuffer;
using Queue = SpscRingBuffer<8, 2, true>;

static void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        std::abort();
    }
}

static void publish(Queue& queue, unsigned n) {
    const float data[] = {static_cast<float>(n), -static_cast<float>(n)};
    check(queue.add_data(data), "publication failed");
}

static void consume(Queue& queue, unsigned n) {
    float data[2]{};
    check(queue.get_latest_data(data), "expected queued frame");
    check(data[0] == static_cast<float>(n) &&
          data[1] == -static_cast<float>(n), "corrupt or unordered frame");
}

static void test_capacity() {
    Queue queue;
    std::atomic<bool> stop{false};
    for (unsigned round = 0; round < 100; ++round) {
        for (unsigned i = 0; i < 8; ++i) publish(queue, round * 8 + i);
        const float rejected[] = {9999, 9999};
        check(!queue.add_data(rejected), "full queue accepted frame");
        for (unsigned i = 0; i < 8; ++i) {
            queue.wait_for_new_data(stop);
            consume(queue, round * 8 + i);
        }
        float empty[2]{};
        check(!queue.get_latest_data(empty), "empty queue returned data");
    }
    SpscRingBuffer<8, 2> output;
    const float frame[] = {1, 2};
    float read[2]{};
    check(output.add_data(frame) && output.get_latest_data(read)
          && read[0] == 1 && read[1] == 2, "non-notifying queue failed");
}

static void test_publication_and_shutdown() {
    for (unsigned i = 0; i < 200; ++i) {
        Queue queue;
        std::atomic<bool> stop{false};
        std::atomic<bool> entered{false};
        std::atomic<bool> consumed{false};
        // Alternate publication before worker creation and racing its wait.
        if (i % 2 == 0) publish(queue, i);
        std::thread worker([&] {
            entered.store(true, std::memory_order_release);
            queue.wait_for_new_data(stop);
            check(!stop.load(std::memory_order_acquire), "unexpected stop");
            consume(queue, i);
            consumed.store(true, std::memory_order_release);
            queue.wait_for_new_data(stop);
            check(stop.load(std::memory_order_acquire), "empty wait returned");
        });
        while (!entered.load(std::memory_order_acquire)) std::this_thread::yield();
        if (i % 2 != 0) publish(queue, i);
        while (!consumed.load(std::memory_order_acquire)) std::this_thread::yield();
        stop.store(true, std::memory_order_release);
        queue.wake_waiters();
        worker.join();
    }
}

static void test_stop_during_processing() {
    Queue queue;
    std::atomic<bool> stop{false}, processing{false}, resume{false};
    publish(queue, 1);
    std::thread worker([&] {
        queue.wait_for_new_data(stop);
        consume(queue, 1);
        processing.store(true, std::memory_order_release);
        while (!resume.load(std::memory_order_acquire)) std::this_thread::yield();
        queue.wait_for_new_data(stop);
        check(stop.load(std::memory_order_acquire), "stop lost during processing");
    });
    while (!processing.load(std::memory_order_acquire)) std::this_thread::yield();
    stop.store(true, std::memory_order_release);
    queue.wake_waiters();
    resume.store(true, std::memory_order_release);
    worker.join();
}

static void test_stress() {
    Queue queue;
    std::atomic<bool> stop{false};
    constexpr unsigned count = 100000;
    std::thread worker([&] {
        for (unsigned i = 0; i < count; ++i) {
            queue.wait_for_new_data(stop);
            consume(queue, i);
        }
    });
    for (unsigned i = 0; i < count; ++i) {
        const float data[] = {static_cast<float>(i), -static_cast<float>(i)};
        // Retrying is test-only; the real-time producer drops full-queue frames.
        while (!queue.add_data(data)) std::this_thread::yield();
    }
    worker.join();
    check(!queue.has_new_data(), "queue not empty after stress test");
}

int main() {
    test_capacity();
    test_publication_and_shutdown();
    test_stop_during_processing();
    test_stress();
    std::cout << "Inference notification regression passed\n";
}
