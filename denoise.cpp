#include "denoise.h"

#include <vector>

namespace {
    const int BAUD_RATE = 300;
    const double MS_PER_CLOCK = 1000.0 / BAUD_RATE;
}

using std::vector;

DeNoiseFilter::DeNoiseFilter(FreqSpanFilter &fs)
    : fs_(fs)
    , spanIdx_(0)
    , eof_(false)
{
    spans_ = fs.getSpans(WINDOW);

    prevSpan_ = getNextSpan();
    if (prevSpan_.value == FreqSpanFilter::Noise) {
        prevSpan_ = getNextSpan();
    }
    currSpan_ = getNextSpan();
}

// Read a stream of frequency spans, some of which are noise. Attempt to
// intelligently combine the noise spans to adjacent spans based on the
// target clock rate of the signal being decoded (i.e. attempt to end up
// with non-noise spans that are near an integral clock width)
//
vector<DeNoiseFilter::Span> DeNoiseFilter::getSpans(int nspans)
{ 
    vector<Span> spans;

    while (spans.size() < nspans && !eof_) {
        if (currSpan_.value != FreqSpanFilter::Noise) {
            double clocks = (prevSpan_.length * 1000.0) / MS_PER_CLOCK;
            prevSpan_.clocks = int(clocks + 0.5);    
            spans.push_back(prevSpan_);
            prevSpan_ = currSpan_;
            currSpan_ = getNextSpan();
            continue;           
        }

        Span nextSpan = getNextSpan();
        if (eof_) {
            break;
        }

        if (dFromClock(prevSpan_.length) > dFromClock(nextSpan.length)) {
            prevSpan_.length += currSpan_.length;
        } else {
            nextSpan.length += currSpan_.length;
        }
        
        double clocks = (prevSpan_.length * 1000.0) / MS_PER_CLOCK;
        prevSpan_.clocks = int(clocks + 0.5);    
        spans.push_back(prevSpan_);
        
        prevSpan_ = currSpan_;
        currSpan_ = nextSpan;
    }

    return spans;
}

// how far from an integral number of clocks is the given
// period in milliseconds
double DeNoiseFilter::dFromClock(double len)
{
    double clk = (len * 1000) / MS_PER_CLOCK;
    clk -= int(clk);
    if (clk > 0.5) {
        clk = 1.0 - clk;
    }

    return clk;
}

// Get the next buffered span
DeNoiseFilter::Span DeNoiseFilter::getNextSpan()
{
    if (eof_) {
        return Span{};
    }
    
    if (spanIdx_ < spans_.size()) {
        return spans_[spanIdx_++];
    }

    spans_ = fs_.getSpans(WINDOW);

    if (spans_.size() == 0) {
        eof_ = true;
        return Span{};
    }

    spanIdx_ = 0;
    return spans_[spanIdx_++];
}      
