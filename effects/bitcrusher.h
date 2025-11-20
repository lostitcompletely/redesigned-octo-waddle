#pragma once


class Bitcrusher {
public:
    Bitcrusher();
    void prepare(int sampleRate);
    float process(float in);
    void reset();
    // simple parameter controls
    void setDownsampleFactor(int f);
    void setBitDepth(int b);
private:
    int sampleRate;
    int downsampleFactor;
    int bitDepth;
    int holdCounter;
    float heldSample;
    static float quantizeSample(float x, int bits);
    static float softLimit(float x);
};