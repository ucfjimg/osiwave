#ifndef BITSTRM_H
#define BITSTRM_H

#include "denoise.h"

class BitstreamFilter {
public:
    using Span = DeNoiseFilter::Span;
    BitstreamFilter(DeNoiseFilter &dn);

    void trace();
    std::vector<bool> getBits(int nbits);
    
private:
    const int WINDOW = 1024;
    
    DeNoiseFilter &dn_;
    std::vector<Span> spans_;
    int spanIdx_;
    bool eof_;
    bool trace_;

    Span span_;
    
    Span getNextSpan();
};

#endif
