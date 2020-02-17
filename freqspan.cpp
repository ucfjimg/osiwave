#include "freqspan.h"

#include "xcross.h"

#include <vector>

using std::vector;

FreqSpanFilter::FreqSpanFilter(ZeroCrossFilter &zc)
    : zc_(zc)
    , eof_(false)
    , zeroCrossingIdx_(0)
{
    zeroCrossings_ = zc_.getTimestamps(WINDOW);
    prevTimestamp_ = getNextZeroCrossing();
    currTimestamp_ = getNextZeroCrossing();
}

// Given zero crossings from the stream, make spans of similar frequency
// in terms of RS-232 marks and spaces.
//
vector<FreqSpanFilter::Span> FreqSpanFilter::getSpans(int nspans)
{
    vector<Span> spans;

    bool first = true;
    double start = prevTimestamp_;
    Value value = Noise;

    while (spans.size() < nspans && !eof_) {
        double dt = currTimestamp_ - prevTimestamp_;
        double freq = 1.0 / dt;

        Value nextValue = Noise;

        // The nominal frequencies are 1200/2400 hz, but things
        // like tape speed, warble, and the waveform not being exactly 
        // centered mean we need to look at ranges.
        //
        if (freq > 2100 && freq < 2550) {
            nextValue = Mark;
        } else if (freq >= 1100 && freq < 1550) {
            nextValue = Space;
        }
        
        if (nextValue != value) {
            if (!first) {
                spans.push_back(Span{ value, currTimestamp_ - start});
            } else {
                first = false;
            }
            value = nextValue;
            start = prevTimestamp_;
        }

        prevTimestamp_ = currTimestamp_;
        currTimestamp_ = getNextZeroCrossing();
    }

    return spans;
}


// Get the next buffered zero crossing.
double FreqSpanFilter::getNextZeroCrossing()
{
    if (eof_) {
        return 0.0;
    }
    
    if (zeroCrossingIdx_ < zeroCrossings_.size()) {
        return zeroCrossings_[zeroCrossingIdx_++];
    }

    zeroCrossings_ = zc_.getTimestamps(WINDOW);

    if (zeroCrossings_.size() == 0) {
        eof_ = true;
        return 0.0;
    }

    zeroCrossingIdx_ = 0;
    return zeroCrossings_[zeroCrossingIdx_++];
}      
