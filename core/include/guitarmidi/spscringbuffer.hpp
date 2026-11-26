#pragma once
/* GuitarMidi-LV2 Library
 * Copyright (C) 2022 Gerald Mwangi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace GuitarMidi {
    template<size_t NumSlots, size_t Stride, bool NotifyOnPublish = false>
    class SpscRingBuffer {
        static_assert(
            NumSlots >= 2 && (NumSlots & (NumSlots - 1)) == 0,
            "NumSlots must be a power of two"
        );

        float buffer[NumSlots][Stride];

        std::atomic<size_t> write_index{0};
        std::atomic<size_t> read_index{0};
        std::atomic<uint64_t> notification_generation{0};

    public:
        // Called only by the producer.
        // Returns false if the buffer is full.
       bool add_data(const float* data, size_t* queued_after = nullptr)
      {
            const size_t write =
                write_index.load(std::memory_order_relaxed);

            const size_t read =
                read_index.load(std::memory_order_acquire);

            if (write - read >= NumSlots) {
                return false;
            }

            std::memcpy(
                buffer[write & (NumSlots - 1)],
                data,
                Stride * sizeof(float)
            );

            write_index.store(
                write + 1,
                std::memory_order_release
            );

            // Publish before signalling; rejected frames never signal.
            if constexpr (NotifyOnPublish) {
                notification_generation.fetch_add(1, std::memory_order_release);
                notification_generation.notify_one();
            }

            #ifdef GUITARMIDI_INFERENCE_DIAGNOSTICS
                if (queued_after != nullptr)
                {
                    *queued_after = write + 1 - read;
                }
            #else
                 (void)queued_after;
            #endif

            return true;
        }

        bool has_new_data() const {
            return write_index.load(std::memory_order_acquire) !=
                   read_index.load(std::memory_order_relaxed);
        }

        // Only the background consumer may wait. Recheck the predicate after
        // sampling the generation to avoid missing publication or shutdown.
        void wait_for_new_data(const std::atomic<bool>& stop) noexcept {
            static_assert(NotifyOnPublish);
            while (!stop.load(std::memory_order_acquire) && !has_new_data()) {
                const auto generation = notification_generation.load(std::memory_order_acquire);
                if (stop.load(std::memory_order_acquire) || has_new_data()) {
                    return;
                }
                notification_generation.wait(generation, std::memory_order_acquire);
            }
        }

        // Shutdown must change the value as well as notifying the waiter.
        void wake_waiters() noexcept {
            static_assert(NotifyOnPublish);
            notification_generation.fetch_add(1, std::memory_order_release);
            notification_generation.notify_all();
        }

        // Called only by the consumer.
        // Returns false if the buffer is empty.
        bool get_latest_data(float* output) {
            const size_t read =
                read_index.load(std::memory_order_relaxed);

            const size_t write =
                write_index.load(std::memory_order_acquire);

            if (read == write) {
                return false;
            }

            std::memcpy(
                output,
                buffer[read & (NumSlots - 1)],
                Stride * sizeof(float)
            );

            read_index.store(
                read + 1,
                std::memory_order_release
            );

            return true;
        }

        #ifdef GUITARMIDI_INFERENCE_DIAGNOSTICS
         size_t queued_count() const noexcept
         {
             const size_t write =
                 write_index.load(std::memory_order_acquire);

             const size_t read =
                 read_index.load(std::memory_order_acquire);

             return write - read;
         }
        #endif
    };

} // namespace GuitarMidi
