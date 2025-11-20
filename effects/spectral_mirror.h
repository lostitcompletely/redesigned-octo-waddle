#pragma once
#include <vector>


class SpectralMirror {
public:
    SpectralMirror();
    void prepare(int sampleRate);
    float process(float in);
    void reset();
private:
    std::vector<float> delayBuffer;
    int writeIndex;
    int delaySamples;
    float delayMs;
    int sampleRate;
};