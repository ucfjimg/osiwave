#include "bitstrm.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using std::cout;
using std::endl;
using std::vector;

BitstreamFilter::BitstreamFilter(DeNoiseFilter &dn)
    : dn_(dn)
    , spanIdx_(0)
    , eof_(false)
    , trace_(false)
{
    spans_ = dn_.getSpans(WINDOW);
    span_ = getNextSpan();
}

// turn on tracing
void BitstreamFilter::trace()
{
    trace_ = true;
}

// Expand spans into individual bits
vector<bool> BitstreamFilter::getBits(int nbits)
{
    vector<bool> bits;

    if (trace_) {
      cout << "bits: ";
    }

    while (bits.size() < nbits && !eof_) {
        while (span_.clocks == 0 && !eof_) {
            span_ = getNextSpan();
        }

        if (eof_) {
          break;
        }

        if (span_.clocks < 0) {
          exit(1);
        }

        if (trace_) {
            cout << (span_.value == FreqSpanFilter::Mark);
        }
        bits.push_back(span_.value == FreqSpanFilter::Mark);
        span_.clocks--;
    }

    if (trace_) {
        cout << endl;
    }

    return bits;
}

// Get the next buffered span.
BitstreamFilter::Span BitstreamFilter::getNextSpan()
{
    if (eof_) {
        return Span{};
    }

    if (spanIdx_ < spans_.size()) {
        Span span = spans_[spanIdx_++];
        return span;
    }

    spans_ = dn_.getSpans(WINDOW);
    spanIdx_ = 0;

    if (spans_.size() == 0) {
        eof_ = true;
        return Span{};
    }

    return getNextSpan();
}
