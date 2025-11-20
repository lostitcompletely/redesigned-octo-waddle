#pragma once


class AutoSwell {
public:
    AutoSwell();
    void prepare(int sampleRate);
    float process(float in);
    void reset();
private:
    float attackTimeSec;
    float threshold;
    float releaseTimeSec;
    float env;
    float prevAbs;
    float attackCoeff;
    float releaseCoeff;
    int sampleRate;
};