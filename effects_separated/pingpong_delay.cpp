// pingpong_delay.cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <portaudio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

constexpr int SAMPLE_RATE = 48000;
constexpr unsigned FRAMES_PER_BUFFER = 256;

// Effect parameters (tweak these constants)
constexpr float DRY_GAIN    = 0.6f;   // direct signal level
constexpr float WET_GAIN    = 0.8f;   // delayed signal level
constexpr float FEEDBACK    = 0.45f;  // feedback amount (0..0.95)
constexpr float LOWPASS_CUT = 6000.0f; // cutoff of feedback lowpass (Hz)
constexpr float DELAY_MS_L  = 350.0f;  // left delay in ms
constexpr float DELAY_MS_R  = 550.0f;  // right delay in ms (ping-pong)

struct Lowpass {
    float a0=1.f, b1=0.f, z1=0.f;
    void setCutoff(float fc) {
        float x = expf(-2.0f * M_PI * fc / SAMPLE_RATE);
        b1 = x;
        a0 = 1.0f - x;
    }
    float process(float in) {
        float y = a0 * in + b1 * z1;
        z1 = y;
        return y;
    }
};

class PingPongDelay {
public:
    PingPongDelay()
    {
        size_t maxSamples = (size_t)(SAMPLE_RATE * 2.0f) + 10; // up to 2s
        bufL.assign(maxSamples, 0.0f);
        bufR.assign(maxSamples, 0.0f);
        writePos = 0;
        // set defaults
        setDelayMs(DELAY_MS_L, DELAY_MS_R);
        fbLowpass.setCutoff(LOWPASS_CUT);
    }

    void setDelayMs(float dLms, float dRms) {
        delaySamplesL = (int)std::round(dLms * 0.001f * SAMPLE_RATE);
        delaySamplesR = (int)std::round(dRms * 0.001f * SAMPLE_RATE);
        if (delaySamplesL < 1) delaySamplesL = 1;
        if (delaySamplesR < 1) delaySamplesR = 1;
        bufMask = (int)bufL.size() - 1;
        // ensure buffer size is power of two for simple wrap (we allocated non-power but mask may be invalid;
        // we'll just wrap manually using modulo).
    }

    // process one mono sample, returns pair<outL, outR>
    void process(float in, float &outL, float &outR) {
        size_t N = bufL.size();

        // read positions (circular)
        int readPosL = (int)writePos - delaySamplesL;
        int readPosR = (int)writePos - delaySamplesR;
        while (readPosL < 0) readPosL += (int)N;
        while (readPosR < 0) readPosR += (int)N;

        float delayedL = bufL[readPosL];
        float delayedR = bufR[readPosR];

        // output is dry + wet mix
        outL = DRY_GAIN * in + WET_GAIN * delayedL;
        outR = DRY_GAIN * in + WET_GAIN * delayedR;

        // Feedback path: mix outputs cross to other channel (ping-pong)
        float fbToL = delayedR * FEEDBACK;
        float fbToR = delayedL * FEEDBACK;

        // lowpass the feedback to avoid bright buildup
        fbToL = fbLowpass.process(fbToL);
        fbToR = fbLowpass.process(fbToR);

        // write new samples into buffer (input + feedback)
        bufL[writePos] = in + fbToL;
        bufR[writePos] = in + fbToR;

        // advance write pointer
        writePos = (writePos + 1) % N;
    }

    void reset() {
        std::fill(bufL.begin(), bufL.end(), 0.0f);
        std::fill(bufR.begin(), bufR.end(), 0.0f);
        writePos = 0;
        fbLowpass.z1 = 0.0f;
    }

private:
    std::vector<float> bufL, bufR;
    size_t writePos;
    int delaySamplesL = 0;
    int delaySamplesR = 0;
    int bufMask = 0;
    Lowpass fbLowpass;
};

// Global instance
static PingPongDelay gDelay;

// PortAudio callback
static int audioCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo*,
                         PaStreamCallbackFlags,
                         void*)
{
    const float* in = (const float*)inputBuffer;
    float* out = (float*)outputBuffer;

    if (!in) {
        std::memset(out, 0, framesPerBuffer * 2 * sizeof(float));
        return paContinue;
    }

    for (unsigned long i = 0; i < framesPerBuffer; ++i) {
        float inS = in[i];

        float l, r;
        gDelay.process(inS, l, r);

        // tiny soft clipping to avoid occasional peak harshness
        auto soft = [](float x)->float {
            const float k = 0.6f;
            return (x / (1.0f + fabsf(x) * k));
        };

        out[2*i + 0] = soft(l);
        out[2*i + 1] = soft(r);
    }

    return paContinue;
}

int main() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio init failed: " << Pa_GetErrorText(err) << "\n";
        return 1;
    }

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
        std::cin.ignore(10000, '\n');
        std::cout << "Using default input device " << inputIndex << "\n";
    } else {
        std::cin.ignore(10000, '\n');
    }

    // configure delay settings (you can tweak here)
    gDelay.setDelayMs(DELAY_MS_L, DELAY_MS_R);

    // lowpass cutoff (feedback)
    // (re-create lowpass with cutoff)
    // done in constructor; if you want different cutoff, adjust in code

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
