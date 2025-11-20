#include "pingpong_delay.h"
#include <vector>
#include <cmath>
#include <algorithm>


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


constexpr float DEFAULT_DELAY_MS_L = 350.0f;
constexpr float DEFAULT_DELAY_MS_R = 550.0f;
constexpr float DRY_GAIN = 0.6f;
constexpr float WET_GAIN = 0.8f;
constexpr float FEEDBACK = 0.45f;


PingPongDelay::PingPongDelay()
: sampleRate(48000), writePos(0), delaySamplesL(1), delaySamplesR(1), fbLowpass_z(0.0f), fbLowpass_a0(1.0f), fbLowpass_b1(0.0f)
{ }


void PingPongDelay::prepare(int sr) {
    sampleRate = sr;
    size_t maxSamples = (size_t)(sampleRate * 2.0f) + 10;
    bufL.assign(maxSamples, 0.0f);
    bufR.assign(maxSamples, 0.0f);
    writePos = 0;
    delaySamplesL = (int)std::round(DEFAULT_DELAY_MS_L * 0.001f * sampleRate);
    delaySamplesR = (int)std::round(DEFAULT_DELAY_MS_R * 0.001f * sampleRate);
    fbLowpass_setCutoff(6000.0f);
}


void PingPongDelay::fbLowpass_setCutoff(float fc) {
    float x = expf(-2.0f * M_PI * fc / sampleRate);
    fbLowpass_b1 = x;
    fbLowpass_a0 = 1.0f - x;
}


float PingPongDelay::fbLowpass_process(float in) {
    float y = fbLowpass_a0 * in + fbLowpass_b1 * fbLowpass_z;
    fbLowpass_z = y;
    return y;
}


void PingPongDelay::process(float in, float &outL, float &outR) {
    size_t N = bufL.size();
    int readPosL = (int)writePos - delaySamplesL;
    int readPosR = (int)writePos - delaySamplesR;
    while (readPosL < 0) readPosL += (int)N;
    while (readPosR < 0) readPosR += (int)N;
    float delayedL = bufL[readPosL];
    float delayedR = bufR[readPosR];
    outL = DRY_GAIN * in + WET_GAIN * delayedL;
    outR = DRY_GAIN * in + WET_GAIN * delayedR;
    float fbToL = fbLowpass_process(delayedR * FEEDBACK);
    float fbToR = fbLowpass_process(delayedL * FEEDBACK);
    bufL[writePos] = in + fbToL;
    bufR[writePos] = in + fbToR;
    writePos = (writePos + 1) % N;
}


void PingPongDelay::reset() {
    std::fill(bufL.begin(), bufL.end(), 0.0f);
    std::fill(bufR.begin(), bufR.end(), 0.0f);
    writePos = 0;
    fbLowpass_z = 0.0f;
}