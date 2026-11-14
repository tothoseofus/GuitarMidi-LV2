#include <guitarmidi/resample_stream.hpp>
#include <speex/speex_resampler.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
constexpr std::uint32_t native_rate = 48000;
constexpr std::uint32_t scratch_size = 512;

void require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

std::vector<float> signal(std::uint32_t rate)
{
    std::vector<float> s(rate * 2); // Two seconds, with noninteger harmonics.
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        const double t = double(i) / rate;
        s[i] = float(0.6 * std::sin(2.0 * 3.141592653589793 * 440.0 * t) +
                     0.2 * std::sin(2.0 * 3.141592653589793 * 1733.0 * t));
    }
    return s;
}

struct Result
{
    std::vector<float> samples;
    std::size_t calls = 0;
    std::size_t max_chunk = 0;
};

Result convert(std::uint32_t rate, const std::vector<float>& input,
               const std::vector<std::uint32_t>& pattern)
{
    int err = 0;
    SpeexResamplerState* state =
        speex_resampler_init(1, rate, native_rate,
                             SPEEX_RESAMPLER_QUALITY_DESKTOP, &err);
    require(state && err == RESAMPLER_ERR_SUCCESS, "initialisation failed");
    std::array<float, scratch_size + 2> scratch{};
    Result result;
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < input.size())
    {
        const std::uint32_t count = std::min<std::size_t>(
            pattern[block++ % pattern.size()], input.size() - offset);
        scratch[0] = 12345.f;
        scratch[scratch_size + 1] = 12345.f;
        bool ok = GuitarMidi::resampleStreamingBlock(
            state, input.data() + offset, count,
            scratch.data() + 1, scratch_size,
            [&](const float* produced, std::uint32_t n, std::uint32_t consumed)
            {
                require(n > 0 && n <= scratch_size, "output chunk out of bounds");
                require(consumed <= count, "input consumption out of bounds");
                result.max_chunk = std::max<std::size_t>(result.max_chunk, n);
                ++result.calls;
                result.samples.insert(result.samples.end(), produced, produced + n);
                return true;
            });
        require(ok, "streaming conversion failed");
        require(scratch[0] == 12345.f && scratch[scratch_size + 1] == 12345.f,
                "scratch guard overwritten");
        offset += count;
    }
    speex_resampler_destroy(state);
    return result;
}

void check(std::uint32_t rate)
{
    const auto input = signal(rate);
    const auto baseline = convert(rate, input, {512});
    const auto irregular = convert(rate, input, {1, 7, 53, 127, 480, 256, 1024, 3});
    const auto tiny = convert(rate, input, {13, 17, 29});

    const auto expected = std::size_t(2 * native_rate);
    for (const auto* r : {&baseline, &irregular, &tiny})
    {
        require(r->samples.size() > expected - 256, "unexpectedly few output samples");
        require(r->samples.size() < expected + 256, "unexpectedly many output samples");
        require(r->calls > 0, "no output callbacks");
        require(r->max_chunk <= scratch_size, "output overflow");
        for (float sample : r->samples)
            require(std::isfinite(sample), "non-finite output");
    }

    require(irregular.samples.size() == baseline.samples.size(),
            "irregular block output count differs");
    require(tiny.samples.size() == baseline.samples.size(),
            "small block output count differs");
    float max_diff = 0.f;
    for (std::size_t i = 0; i < baseline.samples.size(); ++i)
    {
        max_diff = std::max(max_diff,
            std::abs(irregular.samples[i] - baseline.samples[i]));
        max_diff = std::max(max_diff,
            std::abs(tiny.samples[i] - baseline.samples[i]));
    }
    require(max_diff < 1e-4f, "block boundary discontinuity / sample mismatch");
    std::printf("PASS %u -> %u: %zu output frames, max diff %.8g\n",
                rate, native_rate, baseline.samples.size(), max_diff);
}
}

int main()
{
    check(44100);
    check(96000);
    // Native-rate processing bypasses this helper entirely in FretBoard.
    return 0;
}
