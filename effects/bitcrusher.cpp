#include "bitcrusher.h"
#include <cmath>
#include <algorithm>


Bitcrusher::Bitcrusher()
: sampleRate(48000), downsampleFactor(1), bitDepth(8), holdCounter(0), heldSample(0.0f)
{ }


void Bitcrusher::prepare(int sr) {
    sampleRate = sr;
}


float Bitcrusher::quantizeSample(float x, int bits) {
    if (x > 1.0f) x = 1.0f;
    if (x < -1.0f) x = -1.0f;
    float v = (x + 1.0f) * 0.5f;
    int steps = (1 << bits);
    int q = (int) std::floor(v * (steps - 1) + 0.5f);
    float vq = (float)q / (float)(steps - 1);
    return vq * 2.0f - 1.0f;
}


float Bitcrusher::softLimit(float x) {
    float k = 0.6f;
    return x / (1.0f + fabsf(x) * k);
}


float Bitcrusher::process(float in) {
    if (downsampleFactor < 1) downsampleFactor = 1;
    if (bitDepth < 1) bitDepth = 1;
    if (bitDepth > 24) bitDepth = 24;
    if (holdCounter <= 0) {
        heldSample = quantizeSample(in, bitDepth);
        holdCounter = downsampleFactor;
    }
    holdCounter--;
    float wet = heldSample;
    float dry = in;
    float out = 0.6f * dry + 0.9f * wet;
    out *= 0.95f;
    out = softLimit(out);
    return out;
}


void Bitcrusher::reset() {
    holdCounter = 0;
    heldSample = 0.0f;
}


void Bitcrusher::setDownsampleFactor(int f) { if (f >= 1) downsampleFactor = f; }
void Bitcrusher::setBitDepth(int b) { if (b >= 1 && b <= 24) bitDepth = b; }