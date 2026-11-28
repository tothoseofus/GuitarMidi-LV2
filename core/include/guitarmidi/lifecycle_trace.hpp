#pragma once
#include <cstdio>
#ifdef GUITARMIDI_LIFECYCLE_DIAGNOSTICS
#include <thread>
#include <functional>
#endif

// Deliberately independent of host log callbacks. Never call from run/connect_port.
inline void guitarMidiTrace(const char* stage, const void* object,
                            const void* related = nullptr) noexcept
{
#ifdef GUITARMIDI_LIFECYCLE_DIAGNOSTICS
    std::fprintf(stderr, "[GM lifecycle %s] tid=%zu object=%p related=%p %s\n",
        GUITARMIDI_DIAGNOSTIC_REVISION,
        std::hash<std::thread::id>{}(std::this_thread::get_id()),
        const_cast<void*>(object), const_cast<void*>(related), stage);
    std::fflush(stderr);
#else
    (void)stage; (void)object; (void)related;
#endif
}
