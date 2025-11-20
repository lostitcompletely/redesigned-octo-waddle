// bitcrusher.cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <portaudio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------- CONFIG (tweak these) ----------
constexpr int SAMPLE_RATE       = 48000;
constexpr unsigned FRAMES_PER_BUFFER = 256;

// Bitcrusher parameters (tweak to taste)
constexpr int DOWNSAMPLE_FACTOR_DEFAULT = 1;   // integer >=1 (1=no downsample, 8 => hold each sample 8 frames)
constexpr int BIT_DEPTH_DEFAULT         = 8;   // bits (1..24). 8 is classic crunchy.
constexpr float DRY_LEVEL_DEFAULT       = 0.6f; // dry mix
constexpr float WET_LEVEL_DEFAULT       = 0.9f; // wet (crushed) mix
constexpr float OUTPUT_TRIM            = 0.95f; // prevent full-scale clipping

// ---------- Global state ----------
static int downsampleFactor = DOWNSAMPLE_FACTOR_DEFAULT;
static int bitDepth = BIT_DEPTH_DEFAULT;
static float dryLevel = DRY_LEVEL_DEFAULT;
static float wetLevel = WET_LEVEL_DEFAULT;

// runtime state used in callback
struct CrusherState {
    int holdCounter = 0;
    float heldSample = 0.0f;
    int steps = (1 << BIT_DEPTH_DEFAULT);
    float invSteps = 1.0f / (float)(1 << BIT_DEPTH_DEFAULT);
} gState;

// ---------- Helpers ----------
static inline float quantizeSample(float x, int bits) {
    // clamp
    if (x > 1.0f) x = 1.0f;
    if (x < -1.0f) x = -1.0f;
    // map from [-1,1] to [0,1]
    float v = (x + 1.0f) * 0.5f;
    int steps = (1 << bits);
    // round to nearest level
    int q = (int) std::floor(v * (steps - 1) + 0.5f);
    float vq = (float)q / (float)(steps - 1);
    // map back to [-1,1]
    return vq * 2.0f - 1.0f;
}

// gentle soft clip limiter to avoid harsh overloads
static inline float softLimit(float x) {
    // small gain: keep it musical
    float k = 0.6f;
    return x / (1.0f + fabsf(x) * k);
}

// ---------- PortAudio callback ----------
static int audioCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo*,
                         PaStreamCallbackFlags,
                         void*)
{
    const float* in = (const float*)inputBuffer;
    float* out = (float*)outputBuffer;

    if (!in) {
        // Silence on no input
        std::memset(out, 0, framesPerBuffer * 2 * sizeof(float));
        return paContinue;
    }

    // ensure state values are sane per-callback (in case user changes globals externally)
    if (downsampleFactor < 1) downsampleFactor = 1;
    if (bitDepth < 1) bitDepth = 1;
    if (bitDepth > 24) bitDepth = 24;

    for (unsigned long i = 0; i < framesPerBuffer; ++i) {
        float x = in[i]; // mono input expected

        // sample-and-hold (downsample)
        if (gState.holdCounter <= 0) {
            // compute new held sample: first quantize amplitude to bitDepth
            float q = quantizeSample(x, bitDepth);
            gState.heldSample = q;
            gState.holdCounter = downsampleFactor;
        }
        gState.holdCounter--;

        // wet = crushed held sample, dry = original
        float wet = gState.heldSample;
        float dry = x;

        // mix
        float outSample = dryLevel * dry + wetLevel * wet;

        // trim & soft-limit
        outSample *= OUTPUT_TRIM;
        outSample = softLimit(outSample);

        // stereo output
        out[2*i + 0] = outSample;
        out[2*i + 1] = outSample;
    }

    return paContinue;
}

// ---------- Main ----------
int main() {
    // Print defaults and allow the user to change constants in code if desired
    std::cout << "Bitcrusher — defaults: downsample=" << DOWNSAMPLE_FACTOR_DEFAULT
              << " bitdepth=" << BIT_DEPTH_DEFAULT
              << " dry=" << DRY_LEVEL_DEFAULT << " wet=" << WET_LEVEL_DEFAULT << "\n";
    std::cout << "Edit the top of the source to change defaults and recompile.\n\n";

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio init failed: " << Pa_GetErrorText(err) << "\n";
        return 1;
    }

    // list devices
    int devCount = Pa_GetDeviceCount();
    if (devCount < 0) {
        std::cerr << "Pa_GetDeviceCount error: " << devCount << "\n";
        Pa_Terminate();
        return 1;
    }
    std::cout << "Available audio devices:\n";
    for (int i = 0; i < devCount; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        const PaHostApiInfo* host = Pa_GetHostApiInfo(info->hostApi);
        std::cout << "[" << i << "] " << info->name
                  << " (host: " << host->name << ")"
                  << " IN:" << info->maxInputChannels
                  << " OUT:" << info->maxOutputChannels << "\n";
    }

    int inputIndex = Pa_GetDefaultInputDevice();
    std::cout << "\nEnter input device index (default " << inputIndex << "): ";
    if (!(std::cin >> inputIndex)) {
        inputIndex = Pa_GetDefaultInputDevice();
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    } else {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    if (inputIndex < 0 || inputIndex >= devCount) {
        std::cerr << "Invalid device index\n";
        Pa_Terminate();
        return 1;
    }

    // prepare stream parameters
    PaStreamParameters inParams{};
    inParams.device = inputIndex;
    inParams.channelCount = 1; // use mono input
    inParams.sampleFormat = paFloat32;
    inParams.suggestedLatency = Pa_GetDeviceInfo(inputIndex)->defaultLowInputLatency;
    inParams.hostApiSpecificStreamInfo = nullptr;

    PaStreamParameters outParams{};
    outParams.device = Pa_GetDefaultOutputDevice();
    outParams.channelCount = 2; // stereo output
    outParams.sampleFormat = paFloat32;
    outParams.suggestedLatency = Pa_GetDeviceInfo(outParams.device)->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    // initialize runtime state
    gState.holdCounter = 0;
    gState.heldSample = 0.0f;
    gState.steps = (1 << bitDepth);
    gState.invSteps = 1.0f / (float)gState.steps;

    PaStream* stream = nullptr;
    err = Pa_OpenStream(&stream, &inParams, &outParams,
                        SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff,
                        audioCallback, nullptr);
    if (err != paNoError) {
        std::cerr << "Pa_OpenStream failed: " << Pa_GetErrorText(err) << "\n";
        Pa_Terminate();
        return 1;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Pa_StartStream failed: " << Pa_GetErrorText(err) << "\n";
        Pa_CloseStream(stream);
        Pa_Terminate();
        return 1;
    }

    std::cout << "Running... press Enter to stop\n";
    std::cin.get();

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}
