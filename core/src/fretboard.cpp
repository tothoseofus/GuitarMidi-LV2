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
#include <guitarmidi/fretboard.hpp>
#include <guitarmidi/resample_stream.hpp>
#include <omp.h>
#include <algorithm>

using namespace GuitarMidi;

FretBoardAPI* creatFretBoard(GuitarMidiOutput* midioutput){
    return new FretBoard(midioutput);
}
FretBoard::FretBoard(GuitarMidiOutput* midioutput) : m_noteinferencer(midioutput)
{


}

void FretBoard::setAudioInput(const float *input)
{
    if (m_samplerate != NATIVE_SAMPLERATE)
    {
        m_input_buffer = input;
        m_filterbank.setInput(m_resampled_buffer);
    }
    else
    {
        m_filterbank.setInput(input);
    }
}

bool FretBoard::initialize(const std::string &bundle_path, int samplerate, int buffer_size)
{
    g_logger.info("Initializing FretBoard with samplerate %d and buffer size %d ",
                  samplerate, buffer_size);
    if (samplerate <= 0 || buffer_size <= 0)
        return false;

    if (samplerate != NATIVE_SAMPLERATE)
    {
        g_logger.info("Host samplerate %d differs from native %d; enabling SpeexDSP",
                      samplerate, NATIVE_SAMPLERATE);
        int error = RESAMPLER_ERR_SUCCESS;
        m_resampler = speex_resampler_init(
            1, static_cast<spx_uint32_t>(samplerate),
            NATIVE_SAMPLERATE, SPEEX_RESAMPLER_QUALITY_DESKTOP, &error);
        if (!m_resampler || error != RESAMPLER_ERR_SUCCESS)
        {
            g_logger.error("Failed to initialise SpeexDSP resampler");
            return false;
        }
        // SpeexDSP maintains fractional phase and filter history internally.
        // A fixed scratch buffer avoids allocations in the LV2 audio callback.
        m_resampled_buffer = new float[BUFFER_SIZE];
    }

    m_samplerate = samplerate;
    m_fretboard_rep = FretBoardRepresentation();
    m_filterbank.setup(m_fretboard_rep.get_filterrepresentations(), NATIVE_SAMPLERATE, buffer_size);
    m_noteinferencer.setAudioInputBuffer(m_filterbank.get_buffer());
    return m_noteinferencer.initialize(bundle_path);
}

void FretBoard::finalize()
{
    m_noteinferencer.finalize();

    m_filterbank.reset();
}

void FretBoard::setBypassed(bool bypassed)
{
    m_noteinferencer.setBypassed(bypassed);
}

void FretBoard::discardPendingModelOutput()
{
    m_noteinferencer.discardPendingModelOutput();
}

void FretBoard::process(int nsamples)
{
    // prepare the output stream
    m_noteinferencer.preprocess();

    // define the processing lambda
    auto proc=[this](int ns,int p){
        if (m_samplerate != NATIVE_SAMPLERATE)
        {
            process_resampled(ns,p);
        }
        else
        {
            process_direct(ns,p);
        }
    };

    // process once for realtime buffer sizes
    if (nsamples <= BUFFER_SIZE)
    {
        proc(nsamples,0);
    }
    else
    {
        // non realtime, run processing in chunks of BUFFER_SIZE
        int multiple=nsamples/BUFFER_SIZE;
        int rest=nsamples%BUFFER_SIZE;

        for (int m = 0; m < multiple; m++)
        {
            proc(BUFFER_SIZE,m*BUFFER_SIZE);
        }
        // process the rest. Some sound servers provide non log2 buffer sizes like nsamples=480 
        // so here the fractional rest is processed
        if(rest>0)
            proc(rest,multiple*BUFFER_SIZE);
    }

    m_noteinferencer.postprocess();
}



void FretBoard::process_resampled(int nsamples, int pos)
{
    // Exercise the same streaming driver verified by resampling_regression.
    const bool ok = resampleStreamingBlock(
        m_resampler, m_input_buffer + pos, static_cast<std::uint32_t>(nsamples),
        m_resampled_buffer, BUFFER_SIZE,
        [this, pos](const float*, std::uint32_t produced, std::uint32_t consumed)
        {
            // Converted output is already in the preallocated member buffer.
            // MIDI offsets in the resampled path still require runtime validation.
            const int host_pos = pos + (consumed ? static_cast<int>(consumed) - 1 : 0);
            process_48k(static_cast<int>(produced), host_pos);
            return true;
        });
    if (!ok)
        g_logger.error("SpeexDSP streaming conversion failed");
}

void FretBoard::process_48k(int nsamples, int host_pos)
{
    // Resampled output chunks need not coincide with the filterbank's
    // BUFFER_SIZE window boundaries. Never write across one boundary.
    int offset = 0;
    while (offset < nsamples)
    {
        const int remaining_in_window =
            BUFFER_SIZE - m_resampled_window_progress;
        const int count = std::min(nsamples - offset, remaining_in_window);
        m_filterbank.setInput(m_resampled_buffer + offset);
        m_filterbank.process(count, 0);
        m_noteinferencer.process(count, host_pos);
        offset += count;
        m_resampled_window_progress =
            (m_resampled_window_progress + count) % BUFFER_SIZE;
    }
}

void FretBoard::process_direct(int nsamples, int pos)
{
    // Preserve existing 48 kHz processing behaviour exactly.
    m_filterbank.process(nsamples, pos);
    m_noteinferencer.process(nsamples, pos);
}

void FretBoard::getFilterWaveforms(float *waveforms, uint32_t samples_per_filter) const
{
    if (samples_per_filter == 0)
        return;

    const AudioBuffer2D filter_buffer = m_filterbank.get_waveform_buffer();
    for (int filter = 0; filter < NUM_FILTERS; ++filter)
    {
        const float *samples = filter_buffer.audio_buffer_2D + filter * filter_buffer.window_size;
        for (uint32_t point = 0; point < samples_per_filter; ++point)
        {
            const uint32_t sample = point * filter_buffer.window_size / samples_per_filter;
            waveforms[filter * samples_per_filter + point] = samples[sample];
        }
    }
}
