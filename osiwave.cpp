#include "wave.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
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

// the location of zero crossing in the analog data
struct ZeroCrossing {
  int sample;
  double timeStamp;
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

// Given a set of samples, try to clean it up to be symmetric about
// DC (zero).
//
void normalizeAboutDC(vector<int16_t> &data, int dcwin)
{
    vector<int16_t> average;

    int i = 0;
    int sum = 0;
    for (; i < dcwin; i++) {
        sum += data[i];
    }

    for (; i < data.size(); i++) {
        average.push_back(sum / dcwin);
        sum -= data[i-dcwin];
        sum += data[i];
    }

    for (int i = 0; i < average.size(); i++) {
        data[i + dcwin/2] -= average[i];
    }
}

// Given the sample data, return an array of all the zero crossing positions
// in seconds.
//
vector<ZeroCrossing> findZeroCrossings(const vector<int16_t> &data)
{
    vector<ZeroCrossing> zeroCrossings;

    for (int i = 0; i < data.size() - 1; i++) {
        if (data[i] == 0) {
            int s = i;            
            while (i < data.size() - 1 && data[i+1] == 0) {
                i++;
            }
            int nz = i-s+1;

            // interpolate based on multiple zeroes (or exactly 
            // on sample if there's only one)
            double t = i * SEC_PER_SAMPLE + (nz - 1) * 0.5;
            zeroCrossings.push_back(ZeroCrossing{ i, t });
            continue;
        }

        int yl = data[i];
        int yr = data[i+1];


        // if there's a crossing, interpolate
        // the crossing between samples    
        if (yl < 0 && yr > 0) {
            double num = -yl;
            double den = yr - yl;
            double t = num / den;
            t = (i + t) * SEC_PER_SAMPLE;

            zeroCrossings.push_back(ZeroCrossing{ i, t });
        }
    }

    return zeroCrossings;
}

// From zero crossings, compute contiguous spans of our interesting frequencies.
// 1200 hz (or thereabouts) == zero (SPACE)
// 2400 hz == one (MARK)
//
vector<Span> buildFrequencySpans(const vector<ZeroCrossing> &zeroCrossings)
{
    vector<Span> spans;

    Value value = Noise;
    double start = 0;

    int startSample = zeroCrossings[0].sample;
    for (int i = 1; i < zeroCrossings.size(); i++) {
        double dt = zeroCrossings[i].timeStamp - zeroCrossings[i-1].timeStamp;
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
            spans.push_back(Span{ startSample, value, zeroCrossings[i-1].timeStamp - start });
            value = nextValue;
            start = zeroCrossings[i-1].timeStamp;
            startSample = zeroCrossings[i-1].sample;
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

    try {
        data = readWave(waveFile);
    } catch (runtime_error re) {
        cerr << waveFile << ": " << re.what() << endl;
        return 1;
    }

    if (clip >= data.size()) {
        cerr << "clip size " << clip << " would leave no samples." << endl;
    }

    if (clip) {
        cout << "trimming " << clip << " samples from start of wave" << endl;
        data.erase(data.begin(), data.begin() + clip);
    }

    normalizeAboutDC(data, dcwin);
    vector<ZeroCrossing> zeroCrossings = findZeroCrossings(data);

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
