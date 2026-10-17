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
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstring>
#include <semaphore.h>
#include <guitarmidi/common.hpp>
#include <tensorflow/lite/version.h>
#include "tensorflow/core/public/release_version.h"
#include "tensorflow/core/public/version.h"
#include "tensorflow/lite/version.h"
#include "tensorflow/lite/core/interpreter_builder.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model_builder.h"
#include "tensorflow/lite/optional_debug_tools.h"
#include "tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"
using namespace std;
using namespace tflite;
//#define RING_BUFFER_SIZE 8 // Number of frames to store in the ring buffer
/*
    * ModelInferencer class for handling model inference tasks. This class contains two ringbuffers for storing the last N frames of the audio input and model output.
        The audio input ringbuffer is used to store the last N frames of the audio input, which are then fed into the model for inference.
         The model output ringbuffer is used to store the last N frames of the model output, which are then used to in noteinferencer.cpp to trigger MIDI messages based on the confidence of the detected notes.
         The inferencing is performed in a separate thread to avoid blocking the audio processing thread, and the results are communicated back to the main thread using the output ringbuffer.
 */

namespace GuitarMidi{
    //spsc ring buffer for audio input and model output with atomic read and write indices for thread safety
    template<size_t NumSlots, size_t Stride>
    class SpscRingBuffer {
        static_assert(
            NumSlots >= 2 && (NumSlots & (NumSlots - 1)) == 0,
            "NumSlots must be a power of two"
        );

        float buffer[NumSlots][Stride];

        std::atomic<size_t> write_index{0};
        std::atomic<size_t> read_index{0};

    public:
        // Called only by the producer.
        // Returns false if the buffer is full.
        bool add_data(const float* data) {
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

            return true;
        }

        bool has_new_data() const {
            return write_index.load(std::memory_order_acquire) !=
                   read_index.load(std::memory_order_relaxed);
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
    };


/*
    * ModelInferencer class for handling model inference tasks. This class contains two ringbuffers for storing the last N frames of the audio input and model output.
        The audio input ringbuffer is used to store the last N frames of the audio input, which are then fed into the model for inference.
         The model output ringbuffer is used to store the last N frames of the model output, which are then used to in noteinferencer.cpp to trigger MIDI messages based on the confidence of the detected notes.
         The inferencing is performed in a separate thread to avoid blocking the audio processing thread, and the results are communicated back to the main thread using the output ringbuffer.
*/
class ModelInferencer {
    private:
        unique_ptr<FlatBufferModel> m_model;
        unique_ptr<Interpreter> m_interpreter;

        // Must outlive the TensorFlow Lite interpreter.
        TfLiteDelegate* m_xnnpack_delegate = nullptr;

        SpscRingBuffer<RING_BUFFER_SIZE, NUM_NOTES*NUM_HARMONICS*BUFFER_SIZE> audio_input_buffer;
        SpscRingBuffer<RING_BUFFER_SIZE, NUM_NOTES> model_output_buffer;
        std::thread inferencing_thread;

        std::atomic<bool> stop_thread{false};
         sem_t input_ready{};
         bool semaphore_initialized = false;

        /** Runs model inference on frames received by the input ring buffer. */
        void inferencing_loop(const std::string& bundle_path);

        bool initialize_interpreter(const std::string& bundle_path);
         std::promise<bool> m_initialization_promise;
        
    public:
        /** Creates the model inferencer. */
        ModelInferencer();
        /** Stops inference and releases model resources. */
        ~ModelInferencer();
        /** Loads and initializes the model from the plugin bundle. */
        bool initialize(const std::string& bundle_path);
        /** Queues audio frames for asynchronous inference. */
        void add_audio_input(const float* input, int num_frames);
        /** Retrieves inferred note activations for a block of frames. */
        bool get_model_output(float* output, int num_frames);
};
} // namespace GuitarMidi
