#pragma once
#include <vector>
#include <cmath>

class Reverb {
public:
    Reverb();
    void prepare(int sampleRate);
    float process(float in);
    void reset();

private:
    struct Delay {
        std::vector<float> buf;
        int idx = 0;
        float feedback = 0.7f;

        void init(int samples, float fb);
        float process(float in);
        void clear();
    };

    // Comb filters
    Delay combs[4];

    // Allpass filters
    Delay allpasses[2];

    int sr;
};
