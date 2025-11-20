#include "vibrato.h"
#include <cmath>
#include <cstring>


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


Vibrato::Vibrato()
: writeIdx(0), depth(18.0f), rate(2.0f), sampleRate(48000), sampleCounter(0)
{
    std::memset(delayBuf, 0, sizeof(delayBuf));
}


void Vibrato::prepare(int sr) {
    sampleRate = sr;
    writeIdx = 0;
    sampleCounter = 0;
}


float Vibrato::process(float input) {
    delayBuf[writeIdx] = input;
    float lfo = sinf(2.0f * M_PI * rate * (float)sampleCounter / (float)sampleRate);
    float readDelay = depth * lfo + depth; // positive offset
    float readIdx = (float)writeIdx - readDelay;
    while (readIdx < 0) readIdx += MAX_DELAY;
    int i1 = (int)readIdx;
    int i2 = (i1 + 1) % MAX_DELAY;
    float frac = readIdx - (float)i1;
    float out = delayBuf[i1] * (1.0f - frac) + delayBuf[i2] * frac;
    writeIdx = (writeIdx + 1) % MAX_DELAY;
    sampleCounter++;
    return out;
}


void Vibrato::reset() {
    std::memset(delayBuf, 0, sizeof(delayBuf));
    writeIdx = 0;
    sampleCounter = 0;
}