#ifndef FREQSPAN_H
#define FREQSPAN_H

#include <string>
#include <vector>

class ZeroCrossFilter;

class FreqSpanFilter {
public:
    // a value decoded from the analog data
    enum Value {
        Space,   // 1200 hz => zero/space 
        Mark,    // 2400 hz => one/mark
        Noise,   // anything else
    };

    static std::string valueName(Value v);

    // a span of one detected value in the analog data
    struct Span {
        Value value;
        double length;
        int clocks;
    };

    FreqSpanFilter(ZeroCrossFilter &zc);

    void trace();
    std::vector<Span> getSpans(int nspans);

private:
    const int WINDOW = 1024;
    
    ZeroCrossFilter &zc_;
    bool trace_;
    bool eof_;
    std::vector<double> zeroCrossings_;
    int zeroCrossingIdx_;
    double prevTimestamp_;
    double currTimestamp_;

    double getNextZeroCrossing();
};

#endif
