#pragma once
#include <vector>

class PingPongDelay {
public:
    PingPongDelay();
    void prepare(int sampleRate);
    // process mono input -> stereo output
    void process(float in, float &outL, float &outR);
    void reset();
private:
    int sampleRate;
    std::vector<float> bufL, bufR;
    size_t writePos;
    int delaySamplesL, delaySamplesR;
    float fbLowpass_z;
    float fbLowpass_a0, fbLowpass_b1;
    void fbLowpass_setCutoff(float fc);
    float fbLowpass_process(float in);
};