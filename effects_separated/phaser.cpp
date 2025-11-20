// phaser_slow.cpp - realtime slow-sweep phaser (PortAudio + C++)
// Save, compile: g++ phaser_slow.cpp -O2 -lportaudio -o phaser_slow.exe

#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <portaudio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------- CONFIG ----------
constexpr int SAMPLE_RATE = 48000;
constexpr unsigned FRAMES_PER_BUFFER = 256;

// Phaser parameters (tweak these)
int NUM_STAGES = 6;                // number of all-pass stages (4..8 typical)
float LFO_RATE_HZ = 0.18f;         // slow sweep (Hz)
float MIN_FREQ_HZ = 600.0f;        // low end of sweep
float MAX_FREQ_HZ = 2000.0f;       // high end of sweep
float FEEDBACK = 0.30f;            // 0..0.9 (resonance)
float MIX = 0.60f;                 // 0 = dry, 1 = fully wet
float STEREO_PHASE_OFFSET = M_PI/2; // 90 deg between L/R LFO

// ---------- All-pass stage state ----------
struct APStage {
    float x1 = 0.0f; // x[n-1]
    float y1 = 0.0f; // y[n-1]
};

// We'll maintain per-stage states for left and right
static std::vector<APStage> apL, apR;

// LFO state
static double lfoPhase = 0.0;
static double lfoInc = 2.0 * M_PI * LFO_RATE_HZ / SAMPLE_RATE;

// simple clamp
static inline float clampf(float v, float a, float b){ return v < a ? a : (v > b ? b : v); }

// compute first-order all-pass coefficient 'a' for given center freq (approximate mapping)
// using bilinear transform trick: a = (1 - tan(w0/2)) / (1 + tan(w0/2)), with w0 = 2*pi*fc/fs
static inline float coeffForFreq(float fc) {
    // avoid Nyquist / 0
    float nyq = SAMPLE_RATE * 0.5f;
    fc = clampf(fc, 1.0f, nyq - 10.0f);
    float w0 = 2.0f * (float)M_PI * fc / (float)SAMPLE_RATE;
    float t = tanf(w0 * 0.5f);
    // protect against extreme values
    if (!std::isfinite(t)) t = 1e3f;
    float a = (1.0f - t) / (1.0f + t);
    return a;
}

// process one sample through N cascaded first-order all-pass sections (coeffs provided)
static float processAllPassChain(std::vector<APStage> &stages, const std::vector<float> &coeffs, float input) {
    float x = input;
    for (size_t s = 0; s < stages.size(); ++s) {
        float a = coeffs[s];
        // y[n] = -a * x[n] + x[n-1] + a * y[n-1]
        float y = -a * x + stages[s].x1 + a * stages[s].y1;
        // update state
        stages[s].x1 = x;
        stages[s].y1 = y;
        // feed next stage
        x = y;
    }
    return x;
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
        // produce silence
        std::memset(out, 0, framesPerBuffer * 2 * sizeof(float));
        return paContinue;
    }

    // Pre-allocate coeff vectors (per-sample we update them)
    std::vector<float> coeffsL(NUM_STAGES);
    std::vector<float> coeffsR(NUM_STAGES);

    for (unsigned long n = 0; n < framesPerBuffer; ++n) {
        // compute LFO values for left & right (phase offset for stereo)
        float lfoL = 0.5f * (1.0f + std::sinf((float)lfoPhase)); // 0..1
        float lfoR = 0.5f * (1.0f + std::sinf((float)(lfoPhase + STEREO_PHASE_OFFSET)));

        // map to frequency range
        float fcL = MIN_FREQ_HZ + lfoL * (MAX_FREQ_HZ - MIN_FREQ_HZ);
        float fcR = MIN_FREQ_HZ + lfoR * (MAX_FREQ_HZ - MIN_FREQ_HZ);

        // compute per-stage coeffs — small detune across stages optional (we'll stagger slightly)
        for (int s = 0; s < NUM_STAGES; ++s) {
            // optionally stagger center for each stage to widen notches slightly
            float stageOffset = 1.0f + 0.02f * (float)s; // tiny shift
            coeffsL[s] = coeffForFreq(fcL * stageOffset);
            coeffsR[s] = coeffForFreq(fcR * stageOffset);
        }

        // read input
        float x = in[n];

        // add feedback using last output of previous sample: use last y1 of last stage as feedback source
        float lastOutL = apL.back().y1;
        float lastOutR = apR.back().y1;

        float inputL = x + FEEDBACK * lastOutL;
        float inputR = x + FEEDBACK * lastOutR;

        // process through cascaded all-pass stages
        float wetL = processAllPassChain(apL, coeffsL, inputL);
        float wetR = processAllPassChain(apR, coeffsR, inputR);

        // mix wet/dry (and small stereo decorrelation already via phase offset)
        float outL = (1.0f - MIX) * x + MIX * wetL;
        float outR = (1.0f - MIX) * x + MIX * wetR;

        // simple soft-limiter to avoid occasional peaks
        auto soft = [](float v)->float {
            // gentle soft clip
            const float k = 0.9f;
            return v / (1.0f + fabsf(v) * k);
        };

        out[2*n + 0] = soft(outL);
        out[2*n + 1] = soft(outR);

        // advance LFO
        lfoPhase += lfoInc;
        if (lfoPhase >= 2.0 * M_PI) lfoPhase -= 2.0 * M_PI;
    }

    return paContinue;
}

// ---------- Main ----------
int main() {
    // init states
    apL.resize(NUM_STAGES);
    apR.resize(NUM_STAGES);

    // compute lfoInc in case SAMPLE_RATE changed
    lfoInc = 2.0 * M_PI * LFO_RATE_HZ / SAMPLE_RATE;

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio init error: " << Pa_GetErrorText(err) << "\n";
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
        std::cin.ignore(10000, '\n');
    } else {
        std::cin.ignore(10000, '\n');
    }

    if (inputIndex < 0 || inputIndex >= devCount) {
        std::cerr << "Invalid device index\n";
        Pa_Terminate();
        return 1;
    }

    // open stream
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
    err = Pa_OpenStream(&stream, &inParams, &outParams,
                        SAMPLE_RATE, FRAMES_PER_BUFFER,
                        paClipOff, audioCallback, nullptr);
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

    std::cout << "\nPhaser running (slow sweep). Press Enter to stop.\n";
    std::cin.get();

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}
