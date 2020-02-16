#include "xcross.h"

#include "dcfilter.h"

#include <iostream>
#include <vector>

using std::cout;
using std::endl;
using std::vector;

ZeroCrossFilter::ZeroCrossFilter(DCFilter &dc, int sampleRate)
    : dc_(dc)
    , secPerSample_(1.0 / sampleRate)
    , nextSampleIdx_(0)
    , sampleTime_(-1)
    , eof_(false)
{
    samples_ = dc_.readSamples(WINDOW);
    prevSample_ = getNextSample();
}

// Given sample data, find low-to-high zero crossings and return
// their sample timestamps, in seconds, from start of stream.
vector<double> ZeroCrossFilter::getTimestamps(int ncross) 
{
    vector<double> out;
    out.reserve(ncross);

    int l = prevSample_;
    int r = getNextSample();

    while (out.size() < ncross && !eof_) {        
        // if there are a span of zeroes, put the crossing in the middle
        if (l == 0) {
            int zeroes = 1;
            uint32_t s = sampleTime_ - 1;

            while (!eof_ && (l = r) == 0) {
                r = getNextSample();
                zeroes++;
            }

            double t = secPerSample_ *  (s + zeroes * 0.5);
            out.push_back(t);

            r = getNextSample();
            continue;
        }

        if (l < 0 && r > 0) {
            double num = -l;
            double den = r - l;
            double t = num / den;
            t = (sampleTime_ + t) * secPerSample_;

            out.push_back({ t });
        }

        l = r;
        r = getNextSample();
    }

    prevSample_ = r;

    return out;
}

// Get the buffered next sample
//
int ZeroCrossFilter::getNextSample() 
{
    if (eof_) {
        return 0;
    }
    
    if (nextSampleIdx_ < samples_.size()) {
        sampleTime_++;
        return samples_[nextSampleIdx_++];
    }

    samples_ = dc_.readSamples(WINDOW);

    if (samples_.size() == 0) {
        eof_ = true;
        return 0;
    }
    nextSampleIdx_ = 0;

    return getNextSample(); 
}
