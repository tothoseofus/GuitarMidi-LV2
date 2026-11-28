#include <guitarmidi/filterbank.hpp>
#include <guitarmidi/inference_window.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace GuitarMidi;
static void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::abort(); }
}
static std::vector<float> run(const std::vector<int>& blocks, bool split) {
    FilterBank bank;
    std::map<unsigned, FilterRepresentation> reps;
    for (int f = 0; f < NUM_FILTERS; ++f)
        reps.emplace(f, FilterRepresentation{f, 100.0f + f * 10.0f});
    bank.setup(reps, NATIVE_SAMPLERATE, 512);
    float gain = 0;
    bank.setGain(&gain);
#ifdef WITH_AUDIO_OUTPUT
    float note = 0, harmonic = 0;
    std::vector<float> output(1024);
    bank.setNoteSelectControl(&note);
    bank.setHarmonicSelectControl(&harmonic);
    bank.setAudioOutputBuffer(output.data());
#endif
    InferenceWindow<BUFFER_SIZE> submission;
    std::vector<float> snapshots;
    int total = 0, submitted = 0;
    constexpr int length = 8192;
    for (size_t b = 0; total < length; ++b) {
        int n = std::min(blocks[b % blocks.size()], length - total);
        std::vector<float> input(n);
        for (int i = 0; i < n; ++i)
            input[i] = std::sin((total+i)*0.031f);
        bank.setInput(input.data());
        if (!split) {
            bank.process(n);
            total += n;
            check(bank.samplesUntilWindowEnd() == BUFFER_SIZE - total % BUFFER_SIZE, "unsplit offset");
            continue;
        }
        for (int pos = 0; pos < n;) {
            int count = std::min(n-pos, bank.samplesUntilWindowEnd());
            bank.process(count, pos);
            pos += count;
            total += count;
            check(bank.samplesUntilWindowEnd() == BUFFER_SIZE - total % BUFFER_SIZE, "window offset");
            bool submit = submission.advance(count);
            check(submit == (total % (2*BUFFER_SIZE) == 0), "submission cadence");
            if (submit) {
                ++submitted;
                auto buf = bank.get_buffer();
                snapshots.insert(snapshots.end(), buf.audio_buffer_2D,
                                 buf.audio_buffer_2D + NUM_FILTERS*BUFFER_SIZE);
            }
        }
    }
    if (split) check(submitted == length/(2*BUFFER_SIZE), "submission count");
    bank.reset();
    submission.reset();
    check(bank.samplesUntilWindowEnd() == BUFFER_SIZE, "reset offset");
    auto buf = bank.get_buffer();
    check(std::all_of(buf.audio_buffer_2D, buf.audio_buffer_2D + NUM_FILTERS*BUFFER_SIZE,
                      [](float v){return v == 0;}), "reset contents");
    check(!submission.advance(BUFFER_SIZE) && submission.advance(BUFFER_SIZE), "reset cadence");
    return snapshots;
}
int main() {
    const auto reference = run({256}, true);
    for (const auto& blocks : std::vector<std::vector<int>>{{32},{64},{96},{128},{480},{512},{1,97,480,17,256,511}}) {
        const auto actual = run(blocks, true);
        check(actual.size() == reference.size(), "snapshot count");
        for (size_t i = 0; i < actual.size(); ++i)
            check(std::isfinite(actual[i]) && std::abs(actual[i]-reference[i]) <= 1e-6f, "window data differs by host block size");
        run(blocks, false);
    }
    std::cout << "Filterbank window regression passed\n";
}
