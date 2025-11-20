#pragma once


class Vibrato {
public:
    Vibrato();
    void prepare(int sampleRate);
    float process(float in);
    void reset();
private:
    static const int MAX_DELAY = 1024;
    float delayBuf[MAX_DELAY];
    int writeIdx;
    float depth; // in samples
    float rate; // hz
    int sampleRate;
    unsigned long sampleCounter;
};