#include <guitarmidi/midi_frame_time.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

using GuitarMidi::MidiFrameTime;

static void require(bool ok, const char* reason) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", reason);
        std::exit(1);
    }
}

static void checkBlock(std::uint32_t frames) {
    MidiFrameTime timing;
    timing.beginBlock(frames);

    // Requests simulate the irregular progress of asynchronous inference and
    // resampled output callbacks. Every result must remain in host-frame range
    // and events must not move backward inside one Atom sequence.
    const std::array<std::int64_t, 12> requested = {
        -10, 0, 1, 3, 40, 6, 100, 480, 20, 10000, 4, 0
    };
    std::int64_t previous = 0;
    for (auto candidate : requested) {
        const auto t = timing.limit(candidate);
        require(t >= 0, "negative LV2 timestamp");
        require(t <= (frames ? static_cast<std::int64_t>(frames) - 1 : 0),
                "LV2 timestamp outside host block");
        require(t >= previous, "LV2 timestamps moved backward");
        previous = t;
    }

    // The next run block starts a new timeline; no previous timestamp leaks.
    timing.beginBlock(frames);
    require(timing.limit(0) == 0, "block boundary did not reset timing");
    if (frames > 0) {
        timing.beginBlock(frames);
        require(timing.limit(frames - 1) == frames - 1,
                "last valid frame was not preserved");
        require(timing.limit(frames + 50) == frames - 1,
                "overflow frame was not clamped");
    }
}

int main() {
    // Typical native-rate block lengths plus irregular host block lengths.
    for (auto n : {0u, 1u, 7u, 13u, 64u, 127u, 256u, 480u, 512u, 1024u})
        checkBlock(n);

    // Position accounting exercises the source host-frame domain at all rates.
    for (auto rate : {44100u, 48000u, 96000u}) {
        MidiFrameTime timing;
        for (auto block : {1u, 13u, 127u, 256u, 480u, 1024u}) {
            timing.beginBlock(block);
            for (std::uint32_t consumed = 0; consumed <= block; ++consumed) {
                const auto at_end = consumed ? consumed - 1 : 0;
                const auto stamped = timing.limit(at_end);
                require(stamped >= 0 && stamped < block,
                        "resampling timestamp outside host block");
            }
        }
        std::printf("PASS host-frame timestamp bounds: %u Hz\n", rate);
    }
    return 0;
}
