#include "dcfilter.h"

#include "wave.h"

#include <algorithm>
#include <iostream>
#include <vector>

using std::cout;
using std::endl;
using std::vector;

DCFilter::DCFilter(WaveReader &wave, int window)
    : wave_(wave)
    , window_(window)
    , samples_(0)
    , sampleIn_(0)
    , sampleOut_(window / 2)
{
    sampleWindow_ = wave.readSamples(window);
}

// Read some samples. Try to remove DC by subtracting out a windowed
// moving average.
//
vector<int16_t> DCFilter::readSamples(uint32_t nsamples)
{
    // is the entire stream too small to filter?
    if (samples_ == 0 && sampleWindow_.size() < window_) {
        return sampleWindow_;
    }

    vector<int16_t> out;
    out.reserve(nsamples);

    // Handle filling the very first part of the stream before we have enough
    // data to filter. This will get us into steady state where we're returning
    // data from the middle of the window after the offset from the whole window
    // has been applied.
    //
    if (samples_ < window_ / 2) {
        uint32_t n = std::min(window_ / 2 - samples_, nsamples);

        for (uint32_t i = samples_; i < samples_ + n; i++) {
            out.push_back(sampleWindow_[i]);
        }

        samples_ += n;
        nsamples -= n;
    }

    if (nsamples == 0) {
        return out;
    }

    int sum = 0;
    for (int samp : sampleWindow_) {
        sum += samp;
    }

    vector<int16_t> raw = wave_.readSamples(nsamples);

    if (raw.size() == 0) {
        // end of file on source
        int avg = sum / window_;
        
        while (nsamples && sampleOut_ != sampleIn_) {
            out.push_back(sampleWindow_[sampleOut_] - avg);
            sampleOut_ = (sampleOut_ + 1) % window_;
        }

        return out;
    }

    // steady state: use rolling average of samples around a middle sample
    // to filter.
    for (int16_t rawSamp : raw) {
        int avg = sum / window_;
        //cout << window_ << " " << sum << " " << avg << endl;
        int16_t samp = sampleWindow_[sampleOut_] - avg;
        out.push_back(samp);

        sum -= sampleWindow_[sampleIn_];
        sampleWindow_[sampleIn_] = rawSamp;
        sum += rawSamp;

        sampleIn_ = (sampleIn_ + 1) % window_;
        sampleOut_ = (sampleOut_ + 1) % window_;
    }

    return out;
}
