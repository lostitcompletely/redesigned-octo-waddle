#include <iostream>
#include <cmath>
#include <cstring>
#include <vector>
#include <portaudio.h>

#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 256
#define MAX_DELAY_SAMPLES 1024  // max vibrato delay (~20ms)
#define M_PI 3.14159265358979323846

struct Vibrato {
    std::vector<float> delayBuf;
    int writeIdx = 0;

    float depth = 18.0f;     // in samples (~0.1ms per sample)
    float rate = 2.0f;      // Hz

    Vibrato() {
        delayBuf.resize(MAX_DELAY_SAMPLES, 0.0f);
    }

    float process(float input, int sampleIndex) {
        // write input to buffer
        delayBuf[writeIdx] = input;

        // calculate LFO
        float lfo = sinf(2.0f * M_PI * rate * sampleIndex / SAMPLE_RATE);
        float readDelay = depth * lfo + depth; // offset for positive index

        // read index with wrapping
        float readIdx = writeIdx - readDelay;
        if (readIdx < 0) readIdx += MAX_DELAY_SAMPLES;

        // linear interpolation
        int i1 = int(readIdx);
        int i2 = (i1 + 1) % MAX_DELAY_SAMPLES;
        float frac = readIdx - i1;
        float output = delayBuf[i1] * (1.0f - frac) + delayBuf[i2] * frac;

        // increment write index
        writeIdx = (writeIdx + 1) % MAX_DELAY_SAMPLES;

        return output;
    }
};

static Vibrato vibrato;
static unsigned long sampleCounter = 0;

static int audioCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo*,
                         PaStreamCallbackFlags,
                         void*) 
{
    const float* in = (const float*)inputBuffer;
    float* out = (float*)outputBuffer;

    if (!in) {
        memset(out, 0, framesPerBuffer * 2 * sizeof(float));
        return paContinue;
    }

    for (unsigned long i = 0; i < framesPerBuffer; i++) {
        float x = in[i];
        float y = vibrato.process(x, sampleCounter++);

        out[2*i + 0] = y;  // left
        out[2*i + 1] = y;  // right
    }

    return paContinue;
}

int main() {
    Pa_Initialize();

    int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        const PaHostApiInfo* host = Pa_GetHostApiInfo(info->hostApi);
        std::cout << "[" << i << "] " << info->name
                  << " (host: " << host->name << ")"
                  << " IN:" << info->maxInputChannels
                  << " OUT:" << info->maxOutputChannels << "\n";
    }

    int inputIndex;
    std::cout << "\nSelect input device index: ";
    std::cin >> inputIndex;

    PaStreamParameters inParams{};
    inParams.device = inputIndex;
    inParams.channelCount = 1;
    inParams.sampleFormat = paFloat32;
    inParams.suggestedLatency = Pa_GetDeviceInfo(inputIndex)->defaultLowInputLatency;

    PaStreamParameters outParams{};
    outParams.device = Pa_GetDefaultOutputDevice();
    outParams.channelCount = 2;
    outParams.sampleFormat = paFloat32;
    outParams.suggestedLatency = Pa_GetDeviceInfo(outParams.device)->defaultLowOutputLatency;

    PaStream* stream = nullptr;
    Pa_OpenStream(&stream, &inParams, &outParams,
                  SAMPLE_RATE, FRAMES_PER_BUFFER,
                  paClipOff, audioCallback, nullptr);

    Pa_StartStream(stream);
    std::cout << "Vibrato running... press Enter to stop.\n";
    std::cin.ignore();
    std::cin.get();

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}
