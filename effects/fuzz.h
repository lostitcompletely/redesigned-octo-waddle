#pragma once


class Fuzz {
public:
    Fuzz();
    void prepare(int sampleRate);
    float process(float in);
    void reset();
private:
    float inputGain;
    float clipLevel;
    int sampleRate;
    // simple one-pole filters state
    float hpf_z;
    float lpf_z;
    float hpf_a, hpf_b;
    float lpf_a, lpf_b;
    void setHighpass(float cutoff);
    void setLowpass(float cutoff);
    float hpf_process(float in);
    float lpf_process(float in);
};