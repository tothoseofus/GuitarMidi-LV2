#pragma once
namespace GuitarMidi {
// Caller supplies chunks ending at, or before, each filter window boundary.
template<int WindowSize>
class InferenceWindow {
    static_assert(WindowSize > 0);
    int progress = 0;
public:
    void reset() noexcept { progress = 0; }
    bool advance(int samples) noexcept {
        progress += samples;
        if (progress >= 2 * WindowSize) {
            progress -= 2 * WindowSize;
            return true;
        }
        return false;
    }
};
}
