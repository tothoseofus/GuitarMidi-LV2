#pragma once

#include <cstdint>

namespace GuitarMidi {

// Tracks the valid frame-time range of the *current LV2 run block*.
// No allocations, system calls or interaction with the inference engine.
class MidiFrameTime {
public:
    void beginBlock(std::uint32_t frames) noexcept {
        m_block_frames = frames;
        m_last_frame = 0;
    }

    std::int64_t limit(std::int64_t requested) noexcept {
        const std::int64_t maximum = m_block_frames > 0
            ? static_cast<std::int64_t>(m_block_frames) - 1 : 0;
        std::int64_t frame = requested < 0 ? 0 : requested;
        if (frame > maximum) frame = maximum;
        if (frame < m_last_frame) frame = m_last_frame;
        m_last_frame = frame;
        return frame;
    }

private:
    std::uint32_t m_block_frames = 0;
    std::int64_t m_last_frame = 0;
};

} // namespace GuitarMidi
