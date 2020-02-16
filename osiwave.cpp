#include "wave.h"
#include "dcfilter.h"
#include "xcross.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

using std::cerr;
using std::cout;
using std::endl;
using std::ios;
using std::runtime_error;
using std::ifstream;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;

const int SAMPLE_RATE = 44100;
const double SEC_PER_SAMPLE = 1.0 / SAMPLE_RATE;
const int BAUD_RATE = 300;
const double MS_PER_CLOCK = 1000.0 / BAUD_RATE;

// a value decoded from the analog data
enum Value {
    Space,   // 1200 hz => zero/space 
    Mark,    // 2400 hz => one/mark
    Noise,   // anything else
};

// a span of one detected value in the analog data
struct Span {
    int sample;
    Value value;
    double length;
    int clocks;
};

// a bit, with an associated sample index near where it was decoded
struct Bit {
    int sample;
    bool bit;
};

// a bit, with an associated sample index near where it was decoded
struct Char {
    int sample;
    char ch;
};

// Convert a data stream value to a string
//
string valueStr(Value v) 
{
    switch (v) {
    case Space: return "space";
    case Mark:  return "mark";
    case Noise:return "noise";
    }

    return "?unknown";
}


// From zero crossings, compute contiguous spans of our interesting frequencies.
// 1200 hz (or thereabouts) == zero (SPACE)
// 2400 hz == one (MARK)
//
vector<Span> buildFrequencySpans(const vector<double> &zeroCrossings)
{
    vector<Span> spans;

    Value value = Noise;
    double start = 0;

    for (int i = 1; i < zeroCrossings.size(); i++) {
        double dt = zeroCrossings[i] - zeroCrossings[i-1];
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
            spans.push_back(Span{ 0, value, zeroCrossings[i-1] - start });
            value = nextValue;
            start = zeroCrossings[i-1];
        }
    }

    return spans;
}

// Remove noise. Attempt to figure out what the noise value should be
// by looking on either side and seeing which side would make a better
// clock pulse width by adding the noise duration to it.
//
void removeNoise(vector<Span> &spans) 
{
    for (int i = 1; i < spans.size()-1;) {
        if (spans[i].value != Noise) {
            i++;
            continue;
        }

        // how far from an integral number of clocks is the given
        // period in milliseconds
        auto dFromClock = [](double l) {
            double clk = (l * 1000) / MS_PER_CLOCK;
            clk -= int(clk);
            if (clk > 0.5) {
                clk = 1.0 - clk;
            }

            return clk;
        };

        if (dFromClock(spans[i-1].length) > dFromClock(spans[i+1].length)) {
            spans[i-1].length += spans[i].length;
        } else {
            spans[i+1].length += spans[i].length;
        }

        spans.erase(spans.begin() + i);
    }
}

// Removing noise may have introduced adjacent spans that have the same
// value. Find and merge them into one span.
//
void mergeAdjacentSpans(vector<Span> &spans)
{
    for (int i = 0; i < spans.size() - 1;) {
        if (spans[i].value != spans[i+1].value) {
            i++;
            continue;
        }

        spans[i].length += spans[i+1].value;
        spans.erase(spans.begin() + i+1);
    }
}

// Convert the spans to integral numbers of clocks and then convert
// each span to the appropriate number of bits.
//
vector<Bit> convertSpansToBits(vector<Span> &spans)
{
    vector<Bit> sig;

    int clock = 0;
    for (int spi = 0; spi < spans.size(); spi++) {
        auto &sp = spans[spi];
        double clocks = (sp.length * 1000.0) / MS_PER_CLOCK;
        sp.clocks = int(clocks + 0.5);    
        
        bool sval = sp.value == Mark;

        clock += sp.clocks;

        int clk = sp.clocks;
        while (clk--) { sig.push_back(Bit{ sp.sample, sval }); }
    }

    return sig;
}

// Search for RS-232 frames in the data, and convert those frames into
// the original bytes.
//
vector<Char> convertFramesToBytes(const vector<Bit> &sig)
{
    vector<Char> out;

    for (int i = 0; i < sig.size() - 11; i++) {
        // A frame starts after some MARK time, and has a start bit (SPACE),
        // followed by the byte (lsb first), finally followed by a stop 
        // bit (MARK)
        // 
        if (sig[i].bit && !sig[i+1].bit && sig[i+10].bit) {
            char b = 0;

            for (int j = 0; j < 8; j++) {
                if (sig[i+2+j].bit) {
                    b |= (1 << j);
                }
            }

            if (b == 0x0a || b == 0x0d || (b >= 32 && b <= 126)) {  
                out.push_back(Char{ sig[i].sample, b });
            } else if (b != 0x00) {
                // Garbage byte, try to resync on the next clock                
                continue;
            }

            i += 10;
        }
    }

    return out;
}

// Print usage and exit
void usage() 
{
    cerr << "osiwave: [-c clip-samples] [-d dc-window-size] wave-file" << endl;
    exit(1);
}

int main(int argc, char **argv)
{
    int clip = 0;
    int dcwin = 96;
    int opt;

    while ((opt = getopt(argc, argv, "c:d:")) != -1) {
        switch (opt) {
        case 'c':
            clip = atoi(optarg);
            break;
        
        case 'd':
            dcwin = atoi(optarg);
            break;
        
        default:
            usage();
        }
    } 

    if (optind != argc-1) {
        usage();
    }

    string waveFile = argv[optind];
    vector<int16_t> data;

    unique_ptr<WaveReader> reader;

    try {
        reader = unique_ptr<WaveReader>{ new WaveReader{ waveFile } };
        if (reader->getSampleRate() != 44100) {
            cerr << "file must be 44kHz" << endl;
            return 1;
        }
    } catch (runtime_error re) {
        cerr << waveFile << ": " << re.what() << endl;
        return 1;
    }

    // NB we have to do this before constructing the filter chain as 
    // filters may prefill data in their constructors.
    if (clip) {
        reader->skip(clip);
    }


    DCFilter dcFilter{ *reader.get(), dcwin };
    ZeroCrossFilter zeroCross{ dcFilter, reader->getSampleRate() };

    vector<double> zeroCrossings;
    
    while (true) {
        vector<double> chunk = zeroCross.getTimestamps(4096);
        if (chunk.size() == 0) {
            break;
        }

        for (double t : chunk) {
            zeroCrossings.push_back(t);
        }
    }

    if (zeroCrossings.size() == 0) {
        cout << "no data found (no zero crossings)" << endl;
        return 1;
    }

    vector<Span> spans = buildFrequencySpans(zeroCrossings);
    removeNoise(spans);
    mergeAdjacentSpans(spans);
    vector<Bit> sig = convertSpansToBits(spans);
    vector<Char> text = convertFramesToBytes(sig);

    for (Char ch : text) {
        cout << ch.ch;
    }
    cout << endl;

    return 0;
}
