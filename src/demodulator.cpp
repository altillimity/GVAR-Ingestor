#include "demodulator.h"

GVARDemodulator::GVARDemodulator(int samplerate, bool enableThreading) : samplerate_m(samplerate),
                                                                         enableThreading_m(enableThreading),
                                                                         agc(libdsp::AgcCC(0.0010e-3f, 1.0f, 1.0f, 65536)),
                                                                         rrcFilter(libdsp::FIRFilterCCF(1, libdsp::firgen::root_raised_cosine(1, samplerate, 2.11e6, 0.7f, 31))),
                                                                         costasLoop(libdsp::CostasLoop(0.002f, 2)),
                                                                         clockRecoveryMM(libdsp::ClockRecoveryMMCC((float)samplerate / 2.11e6f, pow(8.7e-3, 2) / 4.0, 0.5f, 8.7e-3, 0.005f))
{
    agc_buffer = new std::complex<float>[8192];
    filter_buffer = new std::complex<float>[8192];
    costas_buffer = new std::complex<float>[8192];
    recovery_buffer = new std::complex<float>[8192];
    recovery_real_buffer = new float[8192];
    recovery_bit_buffer = new uint8_t[8192];
    recovery_byte_buffer = new uint8_t[8192];

    if (enableThreading_m)
    {
        agc_buffer2 = new std::complex<float>[8192];
        filter_buffer2 = new std::complex<float>[8192];
        costas_buffer2 = new std::complex<float>[8192];
        recovery_buffer2 = new std::complex<float>[8192];
        recovery_buffer3 = new std::complex<float>[8192];

        agcRun = true;
        rrcRun = true;
        costasRun = true;
        recoveryRun = true;

        agcThread = std::thread(&GVARDemodulator::agcThreadFunc, this);
        rrcThread = std::thread(&GVARDemodulator::rrcThreadFunc, this);
        costasThread = std::thread(&GVARDemodulator::costasThreadFunc, this);
        recoveryThread = std::thread(&GVARDemodulator::recoveryThreadFunc, this);
    }

    stopped = false;
}

void GVARDemodulator::stop()
{
    if (enableThreading_m)
    {
        // Exit all threads... Without causing a race condition!
        agcRun = false;
        agc_pipe.~Pipe();

        if (agcThread.joinable())
            agcThread.join();

        rrcRun = false;
        rrc_pipe.~Pipe();

        if (rrcThread.joinable())
            rrcThread.join();

        costasRun = false;
        costas_pipe.~Pipe();

        if (costasThread.joinable())
            costasThread.join();

        recoveryRun = false;
        recovery_pipe.~Pipe();

        if (recoveryThread.joinable())
            recoveryThread.join();
    }

    stopped = true;
}

GVARDemodulator::~GVARDemodulator()
{
    if (!stopped)
        stop();

    delete[] agc_buffer;
    delete[] filter_buffer;
    delete[] costas_buffer;
    delete[] recovery_buffer;
    delete[] recovery_real_buffer;
    delete[] recovery_bit_buffer;
    delete[] recovery_byte_buffer;

    if (enableThreading_m)
    {
        delete[] agc_buffer2;
        delete[] filter_buffer2;
        delete[] costas_buffer2;
        delete[] recovery_buffer2;
        delete[] recovery_buffer3;
    }
}

void volk_32f_binary_slicer_8i_generic(int8_t *cVector, const float *aVector, unsigned int num_points)
{
    int8_t *cPtr = cVector;
    const float *aPtr = aVector;
    unsigned int number = 0;

    for (number = 0; number < num_points; number++)
    {
        if (*aPtr++ >= 0)
        {
            *cPtr++ = 1;
        }
        else
        {
            *cPtr++ = 0;
        }
    }
}

void GVARDemodulator::pushMultiThread(std::complex<float> *in_samples, int length)
{
    sdr_pipe.push(in_samples, length);
}

int GVARDemodulator::pullMultiThread(uint8_t *out_samples)
{
    int recovery_count = recovery_pipe.pop(recovery_buffer3, 1024);

    if (recovery_count <= 0)
        return 0;

    // Convert to real (BPSK)
    complexToReal.work(recovery_buffer3, recovery_count, recovery_real_buffer);

    // Slice into bits
    volk_32f_binary_slicer_8i_generic((int8_t *)recovery_bit_buffer, recovery_real_buffer, recovery_count);

    // Pack into bytes
    return bitRepacker.work(recovery_bit_buffer, recovery_count, out_samples);
}

int GVARDemodulator::workSingleThread(std::complex<float> *in_samples, int length, uint8_t *out_samples)
{
    // Run AGC
    int agc_count = agc.work(in_samples, length, agc_buffer);

    // Run RRC
    int rrc_count = rrcFilter.work(agc_buffer, agc_count, filter_buffer);

    // Run Costas
    int costas_count = costasLoop.work(filter_buffer, rrc_count, costas_buffer);

    // Run clock recovery
    int recovery_count = clockRecoveryMM.work(costas_buffer, costas_count, recovery_buffer);

    // Convert to real (BPSK)
    complexToReal.work(recovery_buffer, recovery_count, recovery_real_buffer);

    // Slice into bits
    volk_32f_binary_slicer_8i_generic((int8_t *)recovery_bit_buffer, recovery_real_buffer, recovery_count);

    // Pack into bytes
    return bitRepacker.work(recovery_bit_buffer, recovery_count, out_samples);
}

void GVARDemodulator::agcThreadFunc()
{
    while (agcRun)
    {
        int num = sdr_pipe.pop(agc_buffer, 8192);
        if (num <= 0)
            continue;
        agc.work(agc_buffer, num, agc_buffer2);
        agc_pipe.push(agc_buffer2, num);
    }
}

void GVARDemodulator::rrcThreadFunc()
{
    while (rrcRun)
    {
        int num = agc_pipe.pop(filter_buffer, 8192);
        if (num <= 0)
            continue;
        int rrc_samples = rrcFilter.work(filter_buffer, num, filter_buffer2);
        rrc_pipe.push(filter_buffer2, rrc_samples);
    }
}

void GVARDemodulator::costasThreadFunc()
{
    while (costasRun)
    {
        int num = rrc_pipe.pop(costas_buffer, 8192);
        if (num <= 0)
            continue;
        int costas_samples = costasLoop.work(costas_buffer, num, costas_buffer2);
        costas_pipe.push(costas_buffer2, costas_samples);
    }
}

void GVARDemodulator::recoveryThreadFunc()
{
    while (recoveryRun)
    {
        int num = costas_pipe.pop(recovery_buffer, 8192);
        if (num <= 0)
            continue;
        int recovery_samples = clockRecoveryMM.work(recovery_buffer, num, recovery_buffer2);
        recovery_pipe.push(recovery_buffer2, recovery_samples);
    }
}