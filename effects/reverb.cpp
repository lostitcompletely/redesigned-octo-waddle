#include "reverb.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------- Delay implementation ----------------
void Reverb::Delay::init(int samples, float fb) {
    buf.assign(samples, 0.0f);
    idx = 0;
    feedback = fb;
}

float Reverb::Delay::process(float in) {
    if (buf.empty()) return in;

    float out = buf[idx];
    buf[idx] = in + out * feedback;

    idx++;
    if (idx >= (int)buf.size()) idx = 0;

    return out;
}

void Reverb::Delay::clear() {
    std::fill(buf.begin(), buf.end(), 0.0f);
    idx = 0;
}

// ---------------- Reverb implementation ----------------

Reverb::Reverb() : sr(48000) {}

void Reverb::prepare(int sampleRate) {
    sr = sampleRate;

    // Comb delays (prime-ish values for smoothness)
    combs[0].init(int(0.0297f * sr), 0.78f);
    combs[1].init(int(0.0371f * sr), 0.80f);
    combs[2].init(int(0.0411f * sr), 0.82f);
    combs[3].init(int(0.0437f * sr), 0.76f);

    // Allpass delays
    allpasses[0].init(int(0.0050f * sr), 0.70f);
    allpasses[1].init(int(0.0017f * sr), 0.70f);
}

float Reverb::process(float in) {
    // Parallel comb section
    float cSum = 0.0f;
    for (auto &c : combs)
        cSum += c.process(in);

    cSum *= 0.25f;   // normalize comb mix

    // Serial allpass section
    float apOut = cSum;
    for (auto &ap : allpasses) {
        float delayed = ap.process(apOut);
        apOut = delayed * -0.5f + apOut;  // allpass structure
    }

    return apOut;
}

void Reverb::reset() {
    for (auto &c : combs) c.clear();
    for (auto &a : allpasses) a.clear();
}
