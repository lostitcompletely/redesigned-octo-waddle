// exciter.cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <portaudio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------- Config (tweak these) ----------
constexpr int SAMPLE_RATE       = 48000;
constexpr unsigned FRAMES_PER_BUFFER = 256;

// Exciter params (tweak to taste)
constexpr float DRY_GAIN      = 0.6f;   // dry mix level
constexpr float WET_GAIN      = 0.9f;   // wet (processed) mix level
constexpr float DRIVE         = 3.0f;   // saturation drive on highs
constexpr float HIGH_CUTOFF   = 800.0f; // cutoff for low/high split (Hz)
constexpr float SMOOTH_CUTOFF = 12000.0f; // smoothing LP after saturation
constexpr float OUTPUT_TRIM   = 0.95f;  // overall trim to avoid clipping

// ---------- Simple one-pole filter ----------
struct OnePole {
    float a0 = 1.0f;
    float b1 = 0.0f;
    float z1 = 0.0f;
    void setCutoff(float fc) {
        // bilinear-style single-pole alpha (leaky integrator)
        float x = expf(-2.0f * (float)M_PI * fc / (float)SAMPLE_RATE);
        b1 = x;
        a0 = 1.0f - x;
    }
    float process(float in) {
        float y = a0 * in + b1 * z1;
        z1 = y;
        return y;
    }
    void reset() { z1 = 0.0f; }
};

// ---------- Soft saturator (musical) ----------
static inline float softSat(float x) {
    // gentle tanh-like curve but cheaper: cubic soft clip alternative
    // For small x -> approx x, for large x -> compress
    // Using tanhf for best tone if available:
    return tanhf(x);
}

// ---------- Global state ----------
static OnePole lpLow;     // lowpass to extract lows
static OnePole lpSmooth;  // smoothing lowpass after saturation

// ---------- PortAudio callback ----------
static int audioCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*)
{
    const float* in  = (const float*)inputBuffer;
    float* out = (float*)outputBuffer;

    if (!in) {
        std::memset(out, 0, framesPerBuffer * 2 * sizeof(float));
        return paContinue;
    }

    for (unsigned long i = 0; i < framesPerBuffer; ++i) {
        float x = in[i];

        // 1) split: low via one-pole LP, high = residual
        float low = lpLow.process(x);
        float high = x - low;

        // 2) drive + saturate the high band
        float driven = high * DRIVE;
        float shaped = softSat(driven);

        // 3) inverse-gain to keep levels sensible, then smooth
        float shapedScaled = shaped / DRIVE; // bring roughly back to original amplitude
        float shapedSmoothed = lpSmooth.process(shapedScaled);

        // 4) build processed signal: low + enhanced highs
        // optionally emphasize the harmonics by boosting shapedSmoothed a bit
        float enhanced = low + 1.2f * shapedSmoothed;

        // 5) dry/wet mix and trim
        float outSample = DRY_GAIN * x + WET_GAIN * enhanced;
        outSample *= OUTPUT_TRIM;

        // 6) final soft clip to avoid hard digital clipping
        outSample = outSample / (1.0f + fabsf(outSample)); // gentle limiter

        // stereo write
        out[2*i + 0] = outSample;
        out[2*i + 1] = outSample;
    }

    return paContinue;
}

// ---------- Main ----------
int main() {
    // init filters
    lpLow.setCutoff(HIGH_CUTOFF);
    lpSmooth.setCutoff(SMOOTH_CUTOFF);
    lpLow.reset();
    lpSmooth.reset();

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio init failed: " << Pa_GetErrorText(err) << "\n";
        return 1;
    }

    int devCount = Pa_GetDeviceCount();
    if (devCount < 0) {
        std::cerr << "Pa_GetDeviceCount error\n";
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

    // prepare stream params
    PaStreamParameters inParams{};
    inParams.device = inputIndex;
    inParams.channelCount = 1;
    inParams.sampleFormat = paFloat32;
    inParams.suggestedLatency = Pa_GetDeviceInfo(inputIndex)->defaultLowInputLatency;
    inParams.hostApiSpecificStreamInfo = nullptr;

    PaStreamParameters outParams{};
    outParams.device = Pa_GetDefaultOutputDevice();
    outParams.channelCount = 2;
    outParams.sampleFormat = paFloat32;
    outParams.suggestedLatency = Pa_GetDeviceInfo(outParams.device)->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    PaStream* stream = nullptr;
    err = Pa_OpenStream(&stream,
                        &inParams,
                        &outParams,
                        SAMPLE_RATE,
                        FRAMES_PER_BUFFER,
                        paClipOff,
                        audioCallback,
                        nullptr);
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

    std::cout << "Running... press Enter to stop.\n";
    std::cin.get();

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}
