#include "spectral_mirror.h"
#include <algorithm>


SpectralMirror::SpectralMirror()
: writeIndex(0), delaySamples(0), delayMs(2.0f), sampleRate(48000)
{ }


void SpectralMirror::prepare(int sr) {
    sampleRate = sr;
    delaySamples = int((delayMs / 1000.0f) * sampleRate);
    if (delaySamples < 1) delaySamples = 1;
    delayBuffer.assign(sampleRate / 100, 0.0f); // 10ms buffer
    writeIndex = 0;
}


float SpectralMirror::process(float in) {
    if (delayBuffer.empty()) return in;
    int readIndex = writeIndex - delaySamples;
    if (readIndex < 0) readIndex += (int)delayBuffer.size();
    float d = delayBuffer[readIndex];
    delayBuffer[writeIndex] = in;
    writeIndex = (writeIndex + 1) % (int)delayBuffer.size();
    return d - in;
}


void SpectralMirror::reset() {
    std::fill(delayBuffer.begin(), delayBuffer.end(), 0.0f);
    writeIndex = 0;
}