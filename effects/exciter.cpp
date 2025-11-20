#include "exciter.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Exciter::Exciter() : hpState(0.0f), hpCoeff(0.0f), mix(0.4f) {}

void Exciter::prepare(int sampleRate) {
    float cutoff = 3000.0f;
    float x = expf(-2.0f * M_PI * cutoff / sampleRate);
    hpCoeff = x;
}

float Exciter::process(float in) {
    float hp = in - hpState;
    hpState = hpState * hpCoeff + in * (1.0f - hpCoeff);

    float harmonic = tanhf(hp * 4.0f);

    return in * (1.0f - mix) + harmonic * mix;
}

void Exciter::reset() {
    hpState = 0.0f;
}
