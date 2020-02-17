#ifndef DENOISE_H
#define DENOISE_H

#include <vector>

#include "freqspan.h"

class DeNoiseFilter {
public:
    using Span = FreqSpanFilter::Span;

    DeNoiseFilter(FreqSpanFilter &fs);
  
    std::vector<Span> getSpans(int nspans);

private:
    const int WINDOW = 1024;
    
    FreqSpanFilter fs_;

    std::vector<Span> spans_;
    int spanIdx_;
    bool eof_;

    Span prevSpan_;
    Span currSpan_;

    double dFromClock(double len);
    Span getNextSpan();
};

#endif
