    #include <guitarmidi/modelinferencer.hpp>
    #include <iostream>
    #include <tensorflow/lite/logger.h>
    #include <cerrno>
    #include <semaphore.h>
    #include <filesystem>
        // TFlite C++ API reference: https://ai.google.dev/edge/api/tflite/cc?hl=en


    void GuitarMidi::ModelInferencer::inferencing_loop(
        const std::string& bundle_path)
    {
        if (!initialize_interpreter(bundle_path)) {
            g_logger.error("Failed to initialize interpreter.\n");
            m_initialization_promise.set_value(false);
            return;
        }

        m_initialization_promise.set_value(true);

        while (!stop_thread.load(std::memory_order_acquire)) {

            // Sleep until an input frame is available.
            int result;

            do {
                result = sem_wait(&input_ready);
            } while (
                result == -1 &&
                errno == EINTR &&
                !stop_thread.load(std::memory_order_acquire)
            );

            if (stop_thread.load(std::memory_order_acquire)) {
                break;
            }

            if (result == -1) {
                g_logger.error("Inference semaphore wait failed.\n");
                break;
            }

            if (!m_interpreter) {
                continue;
            }

            float* input_buffer =
                m_interpreter->typed_input_tensor<float>(0);

            // Consume exactly one frame for this notification.
            if (!audio_input_buffer.get_latest_data(input_buffer)) {
                continue;
            }

            if (m_interpreter->Invoke() != kTfLiteOk) {
                g_logger.error("TensorFlow Lite inference failed.\n");
                break;
            }    

            float* output_data =
                m_interpreter->typed_output_tensor<float>(0);

            // A full output queue drops the new result.
            model_output_buffer.add_data(output_data);
        }
    }

    bool GuitarMidi::ModelInferencer::initialize_interpreter(const std::string &bundle_path)
    {
                        // Load model
            std::string tflite_path = bundle_path + "/guitarmidi.tflite"; 
            //lv2_log_note(&g_logger, "Loading model from: %s\n", tflite_path.c_str());
            g_logger.info("Loading model from: %s\n", tflite_path.c_str());
            // check if file exists
            if (!std::filesystem::exists(tflite_path)) {
                //lv2_log_error(&g_logger, "Model file not found: %s\n", tflite_path.c_str());
                g_logger.error("Model file not found: %s\n", tflite_path.c_str());
                return false;
            }
            m_model = FlatBufferModel::BuildFromFile(tflite_path.c_str());

            if (!m_model) {
                g_logger.error(
                    "Failed to load TensorFlow Lite model.\n"
                );
                return false;
            }
        
            ops::builtin::BuiltinOpResolver resolver;

            InterpreterBuilder builder(*m_model, resolver);

            if (builder(&m_interpreter) != kTfLiteOk ||
                !m_interpreter)
            {
                g_logger.error(
                    "Failed to create TensorFlow Lite interpreter.\n"
                );
                return false;
            }
        
            TfLiteXNNPackDelegateOptions xnnpack_options =
                TfLiteXNNPackDelegateOptionsDefault();
            xnnpack_options.num_threads = NUM_INFERENCE_THREADS; // Set number of threads as appropriate for your
                                            // platform and application needs.
            xnnpack_options.weight_cache_file_path = TfLiteXNNPackDelegateInMemoryFilePath();
            // xnnpack_options.logging_level = TFLITE_XNNPACK_LOGGING_LEVEL_ERROR;

            // Create and retain the XNNPACK delegate.
            m_xnnpack_delegate =
                TfLiteXNNPackDelegateCreate(&xnnpack_options);

            if (!m_xnnpack_delegate) {
                g_logger.error("Failed to create XNNPACK delegate.\n");
                return false;
            }

            // Attach the delegate to the interpreter.
            if (m_interpreter->ModifyGraphWithDelegate(
                    m_xnnpack_delegate) != kTfLiteOk)
            {
                g_logger.error("Failed to apply XNNPACK delegate.\n");
                return false;
            }
        
            // Allocate tensor buffers.

            if (m_interpreter->AllocateTensors() != kTfLiteOk) {
                g_logger.error(
                    "Failed to allocate TensorFlow Lite tensors.\n"
                );
                return false;
            }
        
            printf("=== Pre-invoke Interpreter State ===\n");
            // tflite::PrintInterpreterState(m_interpreter.get());
            tflite::LoggerOptions::SetMinimumLogSeverity(tflite::TFLITE_LOG_ERROR);
            //lv2_log_note(&g_logger, "Model loaded and interpreter initialized successfully.\n");
            g_logger.info("Model loaded and interpreter initialized successfully.\n");
            //log the ring buffer and inference thread info
            //lv2_log_note(&g_logger, "Inference thread count: %d\n", NUM_INFERENCE_THREADS);
            g_logger.info("Inference thread count: %d\n", NUM_INFERENCE_THREADS);
            // lv2_log_note(&g_logger, "Ring buffer size: %d frames\n", RING_BUFFER_SIZE);
            g_logger.info("Ring buffer size: %d frames\n", RING_BUFFER_SIZE);
            return true;
    }
    
        bool GuitarMidi::ModelInferencer::initialize(
            const std::string& bundle_path)
    {
        if (!semaphore_initialized) {
            return false;
        }

        auto init_future = m_initialization_promise.get_future();

        inferencing_thread = std::thread(
            &ModelInferencer::inferencing_loop,
            this,
            bundle_path
        );

        auto status = init_future.get();

        if (!status) {
            g_logger.error("Interpreter initialization failed.\n");
            return false;
        }

        return true;
    }

    GuitarMidi::ModelInferencer::ModelInferencer()
    {
        semaphore_initialized =
            (sem_init(&input_ready, 0, 0) == 0);

        if (!semaphore_initialized) {
            g_logger.error("Failed to initialise inference semaphore.\n");
        }

        g_logger.info(
            "Audio input ring buffer size: %d frames\n",
            RING_BUFFER_SIZE
        );
    }   

    GuitarMidi::ModelInferencer::~ModelInferencer()
    {
        // Request inference thread shutdown.
        stop_thread.store(true, std::memory_order_release);

        // Wake the thread if it is waiting for input.
        if (semaphore_initialized) {
            sem_post(&input_ready);
        }

        // Wait until inference has completely stopped.
        if (inferencing_thread.joinable()) {
            inferencing_thread.join();
        }

        // The interpreter must be destroyed before the delegate.
        m_interpreter.reset();

        // Release XNNPACK resources.
        if (m_xnnpack_delegate) {
            TfLiteXNNPackDelegateDelete(m_xnnpack_delegate);
            m_xnnpack_delegate = nullptr;
        }

        // Release the model after the interpreter.
        m_model.reset();

        // Finally destroy the input notification semaphore.
        if (semaphore_initialized) {
            sem_destroy(&input_ready);
        }
    }

    void GuitarMidi::ModelInferencer::add_audio_input(
        const float* input,
        int num_frames)
    {
        if (!semaphore_initialized ||
            stop_thread.load(std::memory_order_relaxed)) {
            return;
        }

        // Publish the frame without blocking the audio thread.
        if (audio_input_buffer.add_data(input)) {

            // Wake the inference thread.
            sem_post(&input_ready);
        }

        // If full, drop this new frame.
        // Do not signal because nothing was queued.
    }

    bool GuitarMidi::ModelInferencer::get_model_output(
        float* output,
        int num_frames)
    {
        return model_output_buffer.get_latest_data(output);
    }
