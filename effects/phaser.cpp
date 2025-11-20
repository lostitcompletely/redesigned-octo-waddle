#include "phaser.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Phaser::Phaser() : lfoPhase(0.0f), lfoInc(0.0f), baseDelay(0.0f), depth(0.0f), writeIndex(0) {}

void Phaser::prepare(int sampleRate) {
    buffer.assign(sampleRate * 0.02f, 0.0f);   // 20 ms delay buffer
    writeIndex = 0;

    baseDelay = 0.002f * sampleRate;          // 2 ms
    depth     = 0.0015f * sampleRate;         // ±1.5 ms

    float lfoRate = 0.3f;                     // Hz
    lfoInc = (2.0f * M_PI * lfoRate) / sampleRate;
}

float Phaser::process(float in) {
    if (buffer.empty()) return in;

    float mod = baseDelay + std::sin(lfoPhase) * depth;
    lfoPhase += lfoInc;
    if (lfoPhase >= 2.0f * M_PI) lfoPhase -= 2.0f * M_PI;

    int readIndex = writeIndex - static_cast<int>(mod);
    while (readIndex < 0) readIndex += buffer.size();

    float delayed = buffer[readIndex];
    float out = 0.5f * (in + delayed);

    buffer[writeIndex] = in;
    writeIndex = (writeIndex + 1) % buffer.size();

    return out;
}

void Phaser::reset() {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
    lfoPhase = 0.0f;
}
