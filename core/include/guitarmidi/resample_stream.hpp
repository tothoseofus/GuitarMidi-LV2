#pragma once
// Shared allocation-free streaming driver for the SpeexDSP resampler.
// Owns no memory. The caller creates the state and output scratch at initialise.
#include <speex/speex_resampler.h>
#include <cstdint>

namespace GuitarMidi
{
// Callback receives output samples valid until the next resampler call,
// together with the input frames consumed so far in this host block.
// A false return aborts conversion without making another processing call.
template<class OutputFn>
bool resampleStreamingBlock(
    SpeexResamplerState* state,
    const float* input,
    std::uint32_t input_frames,
    float* scratch,
    std::uint32_t scratch_capacity,
    OutputFn&& on_output)
{
    if (!state || !scratch || !scratch_capacity || (!input && input_frames))
        return false;

    std::uint32_t consumed = 0;
    while (consumed < input_frames)
    {
        spx_uint32_t in_count = input_frames - consumed;
        spx_uint32_t out_count = scratch_capacity;
        const int result = speex_resampler_process_float(
            state, 0, input + consumed, &in_count, scratch, &out_count);
        if (result != RESAMPLER_ERR_SUCCESS)
            return false;

        consumed += in_count;
        if (out_count && !on_output(scratch, out_count, consumed))
            return false;

        if (in_count == 0 && out_count == 0)
            return false;  // No progress: never spin in the audio thread.
    }
    return true;
}
}
