
#pragma once

/*
 * GuitarMidi-LV2
 * Cross-platform inference notification.
 *
 * Uses POSIX semaphores on Linux and native
 * Windows semaphores on Windows.
 *
 * This implementation does not acquire a mutex
 * or wait in the audio-thread notification path.
 *
 * Distributed under the project's LGPL
 * licence terms.
 */

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <semaphore.h>
    #include <cerrno>
#endif

namespace GuitarMidi {

class InferenceNotification {
public:
    InferenceNotification() noexcept {
#ifdef _WIN32
        m_semaphore = CreateSemaphoreW(
            nullptr,  // Default security attributes
            0,        // Initial count
            0x7fffffff, // Maximum count
            nullptr   // Unnamed semaphore
        );
#else
        m_valid = (sem_init(&m_semaphore, 0, 0) == 0);
#endif
    }

    ~InferenceNotification() noexcept {
#ifdef _WIN32
        if (m_semaphore != nullptr) {
            CloseHandle(m_semaphore);
        }
#else
        if (m_valid) {
            sem_destroy(&m_semaphore);
        }
#endif
    }

    InferenceNotification(const InferenceNotification&) = delete;
    InferenceNotification& operator=(const InferenceNotification&) = delete;

    bool valid() const noexcept {
#ifdef _WIN32
        return m_semaphore != nullptr;
#else
        return m_valid;
#endif
    }

    // Called by the audio producer after a frame
    // has been successfully published.
    bool notify() noexcept {
        if (!valid()) {
            return false;
        }

#ifdef _WIN32
        return ReleaseSemaphore(
            m_semaphore, 1, nullptr
        ) != 0;
#else
        return sem_post(&m_semaphore) == 0;
#endif
    }

    // Called only by the inference thread.
    // Blocks until a notification is available.
    bool wait() noexcept {
        if (!valid()) {
            return false;
        }

#ifdef _WIN32
        return WaitForSingleObject(
            m_semaphore, INFINITE
        ) == WAIT_OBJECT_0;
#else
        int result;

        do {
            result = sem_wait(&m_semaphore);
        } while (result == -1 && errno == EINTR);

        return result == 0;
#endif
    }

private:
#ifdef _WIN32
    HANDLE m_semaphore = nullptr;
#else
    sem_t m_semaphore{};
    bool m_valid = false;
#endif
};

} // namespace GuitarMidi
