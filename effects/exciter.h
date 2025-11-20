#pragma once
#include <cmath>

class Exciter {
public:
    Exciter();
    void prepare(int sampleRate);
    float process(float in);
    void reset();

private:
    float hpState;
    float hpCoeff;
    float mix;
};
