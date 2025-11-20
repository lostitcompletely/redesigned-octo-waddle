#include <iostream>
#include <cmath>
#include <cstring>
#include <portaudio.h>

#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 256

// === Auto-Swell Parameters ===
static float attackTimeSec = 0.15f;   // fade-in time (0.1–0.8 recommended)
static float threshold = 0.01f;       // level that triggers a new swell
static float releaseTimeSec = 0.2f;   // envelope release

// === Auto-Swell State ===
static float env = 0.0f;
static float prevAbs = 0.0f;

// Smooth envelope increments
static float attackCoeff;
static float releaseCoeff;

// Audio callback
static int audioCallback(const void *inputBuffer, void *outputBuffer,
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
        float ax = fabsf(x);

        // Detect a fresh note if level jumps above threshold
        if (ax > threshold && prevAbs <= threshold) {
            env = 0.0f; // reset swell
        }
        prevAbs = ax;

        // Envelope generator: attack up, release down
        if (env < 1.0f)
            env += attackCoeff;
        else
            env = 1.0f;

        // Optional: slow fade out after input drops
        if (ax < threshold * 0.5f)
            env -= releaseCoeff;

        if (env < 0.0f) env = 0.0f;
        if (env > 1.0f) env = 1.0f;

        float y = x * env;

        out[i*2 + 0] = y;
        out[i*2 + 1] = y;
    }

    return paContinue;
}

int main() {

    // Precompute envelope speeds
    attackCoeff  = 1.0f / (attackTimeSec * SAMPLE_RATE);
    releaseCoeff = 1.0f / (releaseTimeSec * SAMPLE_RATE);

    Pa_Initialize();

    // List devices
    int num = Pa_GetDeviceCount();
    for (int i = 0; i < num; i++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        std::cout << "[" << i << "] "
                  << info->name
                  << "  IN:" << info->maxInputChannels
                  << "  OUT:" << info->maxOutputChannels << "\n";
    }

    int inputIndex;
    std::cout << "\nSelect INPUT device: ";
    std::cin >> inputIndex;

    PaStreamParameters inP{}, outP{};
    inP.device = inputIndex;
    inP.channelCount = 1;
    inP.sampleFormat = paFloat32;
    inP.suggestedLatency =
        Pa_GetDeviceInfo(inputIndex)->defaultLowInputLatency;

    outP.device = Pa_GetDefaultOutputDevice();
    outP.channelCount = 2;
    outP.sampleFormat = paFloat32;
    outP.suggestedLatency =
        Pa_GetDeviceInfo(outP.device)->defaultLowOutputLatency;

    PaStream* stream;
    Pa_OpenStream(&stream, &inP, &outP,
                  SAMPLE_RATE, FRAMES_PER_BUFFER,
                  paNoFlag, audioCallback, nullptr);

    Pa_StartStream(stream);

    std::cout << "\nRunning Auto-Swell… press Enter to quit.\n";
    std::cin.ignore();
    std::cin.get();

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    return 0;
}
