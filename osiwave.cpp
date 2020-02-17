#include "wave.h"
#include "dcfilter.h"
#include "xcross.h"
#include "freqspan.h"
#include "denoise.h"

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

using Span = FreqSpanFilter::Span;

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

// Convert the spans to integral numbers of clocks and then convert
// each span to the appropriate number of bits.
//
vector<Bit> convertSpansToBits(vector<Span> &spans)
{
    vector<Bit> sig;

    int clock = 0;
    for (int spi = 0; spi < spans.size(); spi++) {
        auto &sp = spans[spi];
        
        bool sval = sp.value == FreqSpanFilter::Mark;

        clock += sp.clocks;

        int clk = sp.clocks;
        while (clk--) { sig.push_back(Bit{ 0, sval }); }
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
    FreqSpanFilter freqSpan{ zeroCross };
    DeNoiseFilter denoise{ freqSpan };

    vector<FreqSpanFilter::Span> spans;
    
    while (true) {
        vector<FreqSpanFilter::Span> chunk = denoise.getSpans(4096);
        if (chunk.size() == 0) {
            break;
        }

        for (auto  t : chunk) {
            spans.push_back(t);
        }
    }

    cout << spans.size() << " spans" << endl;

    vector<Bit> sig = convertSpansToBits(spans);
    vector<Char> text = convertFramesToBytes(sig);

    for (Char ch : text) {
        cout << ch.ch;
    }
    cout << endl;

    return 0;
}
