#pragma once
#include <vector>
#include <cmath>

class Phaser {
public:
    Phaser();
    void prepare(int sampleRate);
    float process(float in);
    void reset();

private:
    float lfoPhase;
    float lfoInc;
    float baseDelay;
    float depth;

    std::vector<float> buffer;
    int writeIndex;
};
