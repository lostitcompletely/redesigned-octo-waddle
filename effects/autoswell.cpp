#include "autoswell.h"
#include <cmath>


AutoSwell::AutoSwell()
: attackTimeSec(0.15f), threshold(0.01f), releaseTimeSec(0.2f), env(0.0f), prevAbs(0.0f), sampleRate(48000)
{ }


void AutoSwell::prepare(int sr) {
sampleRate = sr;
attackCoeff = 1.0f / (attackTimeSec * sampleRate);
releaseCoeff = 1.0f / (releaseTimeSec * sampleRate);
}


float AutoSwell::process(float in) {
float x = in;
float ax = fabsf(x);
if (ax > threshold && prevAbs <= threshold) env = 0.0f;
prevAbs = ax;
if (env < 1.0f) env += attackCoeff; else env = 1.0f;
if (ax < threshold * 0.5f) env -= releaseCoeff;
if (env < 0.0f) env = 0.0f;
if (env > 1.0f) env = 1.0f;
return x * env;
}


void AutoSwell::reset() {
env = 0.0f; prevAbs = 0.0f;
}