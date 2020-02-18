#ifndef XCROSS_H
#define XCROSS_H

#include <cstdint>
#include <vector>

class DCFilter;

class ZeroCrossFilter {
public:
    ZeroCrossFilter(DCFilter &dc, int sampleRate, bool negate);

    void trace();
    std::vector<double> getTimestamps(int ncross);

private:
    const uint32_t WINDOW = 4096;

    DCFilter& dc_;
    bool trace_;
    bool negate_;
    double secPerSample_;
    std::vector<int16_t> samples_;
    int nextSampleIdx_;
    uint32_t sampleTime_;
    bool eof_;
    int prevSample_;

    int getNextSample();
};

#endif
