#include "wave.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using std::ifstream;
using std::ios;
using std::ofstream;
using std::runtime_error;
using std::stringstream;
using std::string;
using std::vector;

const int PCM_TAG = 1;

// Open the file and verify the format
//
WaveReader::WaveReader(const string &fname)
    : sampleRate_(44100)
    , nchannels_(1)
    , readChan_(0)
    , endOfData_(0)
{
    const string badFile = "file is not a wave file.";

    in_.open(fname, ios::binary|ios::ate);
    if (!in_) {
        throw runtime_error{ "failed to open file." };
    }

    auto fileSize = in_.tellg();
    in_.seekg(0);

    if (readFourCC() != "RIFF") {
        throw runtime_error{ badFile };
    }

    uint32_t riffChunkSize = readUnsignedWord(4);

    // riffChunkSize is how many bytes is in the WAVE chunk, less the
    // 8 bytes for RIFF and the following uint32.
    //
    if (riffChunkSize + 8 > fileSize || riffChunkSize < 4 || readFourCC() != "WAVE") {
        throw runtime_error{ badFile };
    }

    uint32_t left = riffChunkSize - 4;

    uint32_t dataStart = 0;

    // walk the chunks
    //
    while (left > 8) {
        string fcc = readFourCC();
        uint32_t len = readUnsignedWord(4);
        left -= 8;

        if (len > left) {
            throw runtime_error{ badFile };
        }
        left -= len;

        if (fcc == "data") {
            dataStart = in_.tellg();
            endOfData_ = dataStart + len;
            continue;
        }

        if (fcc == "fmt ") {
            // 16 is the min size for the format struct
            if (len < 16) {
                throw runtime_error{ badFile };
            }
            // see: WAVEFORMATEX from Win32
            uint32_t formatTag = readUnsignedWord(2);
            nchannels_ = readUnsignedWord(2);
            sampleRate_ = readUnsignedWord(4);
            uint32_t bytesPerSec = readUnsignedWord(4);
            uint32_t blockAlign = readUnsignedWord(2);
            uint32_t bitsPerSample = readUnsignedWord(2);

            // the two restrictions we have on the format are that we only
            // handle 16-bit PCM.
            //
            const int PCM_FORMAT = 1;
            if (formatTag != PCM_FORMAT || bitsPerSample != 16) {
                throw runtime_error{ "wave format must be 16-bit PCM." };
            }
        }
    }

    if (dataStart == 0) {
        // we never found a data chunk
        throw runtime_error{ badFile };
    }
    
    in_.seekg(dataStart);
}

// Sets the read channel, which is zero based. If the stream is stereo,
// 0 is left and 1 is right.
//
void WaveReader::setReadChannel(int chan) 
{
    if (chan < 0 || chan >= nchannels_) {
        stringstream ss;
        ss
            << "attempt to set invalid read channel "
            << chan
            << " -- stream only has "
            << nchannels_
            << " channels.";
        throw runtime_error{ ss.str() };
    }

    readChan_ = chan;
}

// Reads a block of samples of size `nsamples'. Fewer samples may
// be returned. After all samples have been read, an empty vector
// will be returned.
//
vector<int16_t> WaveReader::readSamples(uint32_t nsamples)
{
    vector<int16_t> data;

    int stride = sizeof(int16_t) * nchannels_;
    uint32_t nbytes = nsamples * stride;
    uint32_t left = endOfData_ - in_.tellg();

    if (left == 0) {
        return data;
    }

    nbytes = std::min(nbytes, left);

    if (readBuf_.size() < nbytes) {
        readBuf_.resize(nbytes);
    }

    in_.read(readBuf_.data(), nbytes);
    if (in_.fail()) {
        throw runtime_error{ "failed reading samples from stream." };
    }

    nsamples = nbytes / stride;
    data.reserve(nsamples);

    int offs = sizeof(int16_t) * readChan_;

    for (uint32_t i = 0; i < nsamples; i++) {
        uint16_t lo = readBuf_[i * stride + offs] & 0xff;
        uint16_t hi = readBuf_[i * stride + offs + 1] & 0xff;
        data.push_back(static_cast<int16_t>((hi << 8) | lo));
    }

    return data;
}


// Read a FourCC code
//
string WaveReader::readFourCC()
{
    char fcc[5];

    in_.read(fcc, 4);
    if (in_.fail()) {
        throw runtime_error{ "premature end of file on wave file." };
    }
    fcc[4] = '\0';
    return fcc;
}

// Read a little-endian word of up to 4 bytes
//
uint32_t WaveReader::readUnsignedWord(int nbytes)
{
    uint32_t ui = 0;
    char bytes[4];

    in_.read(bytes, nbytes);
    if (in_.fail()) {
        throw runtime_error{ "premature end of file on wave file." };
    }

    for (int i = nbytes-1; i >= 0; i--) {
        ui <<= 8;
        ui += static_cast<uint8_t>(bytes[i]);
    }

    return ui;
}
