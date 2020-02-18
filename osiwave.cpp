#include "wave.h"
#include "dcfilter.h"
#include "xcross.h"
#include "freqspan.h"
#include "denoise.h"
#include "bitstrm.h"
#include "frameflt.h"

#include <iostream>
#include <vector>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include <unistd.h>

using std::cerr;
using std::cout;
using std::endl;
using std::runtime_error;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

using Span = FreqSpanFilter::Span;

// Print usage and exit
void usage() 
{
    cerr << "osiwave: [-c clip-samples] [-d dc-window-size] [-n] wave-file" << endl;
    exit(1);
}

int main(int argc, char **argv)
{
    int clip = 0;
    int dcwin = 96;
    int opt;
    set<char> trace;
    bool negateZeroCross = false;

    while ((opt = getopt(argc, argv, "c:d:nt:")) != -1) {
        switch (opt) {
        case 'c':
            clip = atoi(optarg);
            break;
        
        case 'd':
            dcwin = atoi(optarg);
            break;

        case 'n':
            negateZeroCross = true;
            break;

        case 't':
            for (char *pch = optarg; *pch; pch++) {
                trace.insert(*pch);
            }
            break;
        
        default:
            usage();
        }
    } 

    if (optind != argc-1) {
        usage();
    }

    auto traceClass = [&](char ch) {
        return trace.find(ch) != trace.end();
    };

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
    ZeroCrossFilter zeroCross{ dcFilter, reader->getSampleRate(), negateZeroCross };
    if (traceClass('z')) { zeroCross.trace(); }

    FreqSpanFilter freqSpan{ zeroCross };
    if (traceClass('f')) { freqSpan.trace(); }

    DeNoiseFilter denoise{ freqSpan };
    BitstreamFilter bitstream{ denoise };
    if (traceClass('b')) { bitstream.trace(); }

    FrameFilter frames{ bitstream };


    while (true) {
        vector<char> chunk = frames.getChars(4096);
        if (chunk.size() == 0) {
            break;
        }

        for (char t : chunk) {
            cout << t;
        }
    }

    cout << endl;

    return 0;
}
