#ifndef DCFILTER_H
#define DCFILTER_H

#include <cstdint>
#include <vector>

class WaveReader;

class DCFilter {
public:
    DCFilter(WaveReader& wave, int window);

    std::vector<int16_t> readSamples(uint32_t nsamples);

private:
    WaveReader &wave_;
    int window_;
    std::vector<int16_t> sampleWindow_;
    uint32_t samples_;
    uint32_t sampleIn_;
    uint32_t sampleOut_;
};

#endif

