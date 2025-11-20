#include <iostream>
#include <cmath>
#include <vector>
#include <portaudio.h>
#include <cstring>

#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 256

// Delay time in ms
static float delayMs = 2.0f;     // change this to taste
static int delaySamples;

// Circular buffer
static std::vector<float> delayBuffer;
static int writeIndex = 0;

static int audioCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void*)
{
    const float* in = (const float*)inputBuffer;
    float* out = (float*)outputBuffer;

    if (!in) {
        memset(out, 0, sizeof(float) * framesPerBuffer * 2);
        return paContinue;
    }

    for (unsigned long i = 0; i < framesPerBuffer; i++) {
        float x = in[i];

        int readIndex = writeIndex - delaySamples;
        if (readIndex < 0) readIndex += delayBuffer.size();

        float d = delayBuffer[readIndex];

        delayBuffer[writeIndex] = x;
        writeIndex++;
        if (writeIndex >= delayBuffer.size()) writeIndex = 0;

        float y = d - x;  // spectral mirror magic
        out[i*2 + 0] = y;
        out[i*2 + 1] = y;
    }

    return paContinue;
}

int main() {
    delaySamples = int((delayMs / 1000.0f) * SAMPLE_RATE);
    delayBuffer.resize(SAMPLE_RATE * 0.01);  // 10ms buffer

    Pa_Initialize();

    int devCount = Pa_GetDeviceCount();
    for (int i = 0; i < devCount; i++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        std::cout << "[" << i << "] " << info->name
                  << " IN:" << info->maxInputChannels
                  << " OUT:" << info->maxOutputChannels << "\n";
    }

    int inputIndex;
    std::cout << "\nSelect INPUT device: ";
    std::cin >> inputIndex;

    PaStreamParameters inP{}, outP{};
    inP.device = inputIndex;
    inP.channelCount = 1;
    inP.sampleFormat = paFloat32;
    inP.suggestedLatency = Pa_GetDeviceInfo(inputIndex)->defaultLowInputLatency;

    outP.device = Pa_GetDefaultOutputDevice();
    outP.channelCount = 2;
    outP.sampleFormat = paFloat32;
    outP.suggestedLatency = Pa_GetDeviceInfo(outP.device)->defaultLowOutputLatency;

    PaStream* stream;
    Pa_OpenStream(&stream, &inP, &outP,
                  SAMPLE_RATE, FRAMES_PER_BUFFER, paNoFlag,
                  audioCallback, nullptr);

    Pa_StartStream(stream);
    std::cout << "Spectral mirror running... press Enter\n";
    std::cin.ignore();
    std::cin.get();

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}