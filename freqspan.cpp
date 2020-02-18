#include "freqspan.h"

#include "xcross.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::string;
using std::vector;

string FreqSpanFilter::valueName(Value v)
{
    switch(v) {
        case Mark: return "mark";
        case Space: return "space";
        case Noise: return "noise";
    }
}


FreqSpanFilter::FreqSpanFilter(ZeroCrossFilter &zc)
    : zc_(zc)
    , trace_(false)
    , eof_(false)
    , zeroCrossingIdx_(0)
{
    zeroCrossings_ = zc_.getTimestamps(WINDOW);
    prevTimestamp_ = getNextZeroCrossing();
    currTimestamp_ = getNextZeroCrossing();
}

// Enable tracing
void FreqSpanFilter::trace()
{
    trace_ = true;
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

    if (trace_) {
        cout << "frequency spans" << endl;
        cout << "prev " << prevTimestamp_ << endl;
    }

    while (spans.size() < nspans && !eof_) {
        if (trace_) {
            cout << "curr " << currTimestamp_ << endl;
        }
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
                double dt = currTimestamp_ - start;
                if (trace_) {
                    cout << "  " << freq << "  " << valueName(value) << " -> " << valueName(nextValue) << dt << endl;
                }
                spans.push_back(Span{ value, dt });
            } else {  
                if (trace_) {
                    cout << "first" << endl;
                }
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
