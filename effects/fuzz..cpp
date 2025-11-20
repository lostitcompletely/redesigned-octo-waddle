#include "fuzz.h"
#include <cmath>
#include <algorithm>


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


Fuzz::Fuzz()
: inputGain(80.0f), clipLevel(0.08f), sampleRate(48000),
hpf_z(0.f), lpf_z(0.f), hpf_a(0.f), hpf_b(0.f), lpf_a(0.f), lpf_b(0.f)
{ }


void Fuzz::prepare(int sr) {
    sampleRate = sr;
    setHighpass(100.0f);
    setLowpass(800.0f);
}


void Fuzz::setHighpass(float cutoff) {
    float x = expf(-2.0f * M_PI * cutoff / (float)sampleRate);
    hpf_a = (1.0f + x) * 0.5f;
    hpf_b = x;
}


void Fuzz::setLowpass(float cutoff) {
    float x = expf(-2.0f * M_PI * cutoff / (float)sampleRate);
    lpf_a = 1.0f - x;
    lpf_b = x;
}


float Fuzz::hpf_process(float in) {
    hpf_z = hpf_a * (in - hpf_z) + hpf_z * hpf_b;
    return hpf_z;
}


float Fuzz::lpf_process(float in) {
    lpf_z = lpf_a * in + lpf_b * lpf_z;
    return lpf_z;
}


float Fuzz::process(float in) {
    float x = in;
    x = hpf_process(x);
    x *= inputGain;
    if (x > clipLevel) x = clipLevel;
    if (x < -clipLevel) x = -clipLevel;
    x = lpf_process(x);
    return x;
}


void Fuzz::reset() {
    hpf_z = lpf_z = 0.0f;
}