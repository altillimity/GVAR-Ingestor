#pragma once

#include <thread>
#include <complex>
#include <dsp/agc.h>
#include <dsp/pipe.h>
#include <dsp/fir_filter.h>
#include <dsp/fir_gen.h>
#include <dsp/costas_loop.h>
#include <dsp/clock_recovery_mm.h>
#include <dsp/complex_to_real.h>
#include "processing/repack_bits_byte.h"

class GVARDemodulator
{
private:
    // Settings
    const int samplerate_m;
    const bool enableThreading_m;

    // DSP Blocks
    libdsp::AgcCC agc;
    libdsp::FIRFilterCCF rrcFilter;
    libdsp::CostasLoop costasLoop;
    libdsp::ClockRecoveryMMCC clockRecoveryMM;
    libdsp::ComplexToReal complexToReal;
    RepackBitsByte bitRepacker;

    // Buffers to work with
    std::complex<float> *agc_buffer,
        *filter_buffer,
        *costas_buffer,
        *recovery_buffer;
    std::complex<float> *agc_buffer2,
        *filter_buffer2,
        *costas_buffer2,
        *recovery_buffer2,
        *recovery_buffer3;
    float *recovery_real_buffer;
    uint8_t *recovery_bit_buffer;
    uint8_t *recovery_byte_buffer;

    // Piping stuff if we need it
    libdsp::Pipe<std::complex<float>> sdr_pipe;
    libdsp::Pipe<std::complex<float>> agc_pipe;
    libdsp::Pipe<std::complex<float>> rrc_pipe;
    libdsp::Pipe<std::complex<float>> costas_pipe;
    libdsp::Pipe<std::complex<float>> recovery_pipe;

    // Threading
    std::thread agcThread, rrcThread, costasThread, recoveryThread;

    // Thread bools
    bool agcRun, rrcRun, costasRun, recoveryRun;
    bool stopped;

private:
    void agcThreadFunc();
    void rrcThreadFunc();
    void costasThreadFunc();
    void recoveryThreadFunc();

public:
    GVARDemodulator(int samplerate, bool enableThreading = false);
    ~GVARDemodulator();
    int workSingleThread(std::complex<float> *in_samples, int length, uint8_t *out_samples);
    void pushMultiThread(std::complex<float> *in_samples, int length);
    int pullMultiThread(uint8_t *out_samples);
    void stop();
};