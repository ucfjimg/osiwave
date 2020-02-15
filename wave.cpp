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

// read the entire binary file into memory, throw an exception
// on error.
//
vector<char> readFile(const string &fname) 
{
    vector<char> contents;

    ifstream in{ fname, ios::binary | ios::ate };
    if (!in) {
        stringstream ss;
        ss << "Could not open `" << fname << "'.";
        throw runtime_error{ ss.str() };
    }

    auto nbytes = in.tellg();
    in.seekg(0);

    contents.resize(nbytes);
    in.read(contents.data(), nbytes);

    return contents;
}

// Check for a RIFF fourcc value at a given location in the file.
//
bool checkFourCC(const vector<char>& data, int at, const char *fcc2)
{
    for (int i = 0; i < 4; i++) {
        if (data[at + i] != fcc2[i]) {
            return false;
        }
    }
 
    return true;
}

// Extract a fourcc value at a given location in the file.
//
string extractFourCC(const vector<char>& data, int at)
{
    string fourcc;

    for (int i = 0; i < 4; i++) {
        fourcc += data[at + i];
    }

    return fourcc;
}

// Extract a little-endian 16-bit value from the file.
//
uint16_t extract16(const vector<char>& data, int at)
{
    uint16_t result = 0;

    for (int i = 1; i >= 0; i--) {
        uint8_t b = static_cast<uint8_t>(data[at + i]);
        result = result * 256 + b;
    }

    return result;
}

// Extract a little-endian 32-bit value from the file.
//
uint32_t extract32(const vector<char>& data, int at)
{
    uint32_t result = 0;

    for (int i = 3; i >= 0; i--) {
        uint8_t b = static_cast<uint8_t>(data[at + i]);
        result = result * 256 + b;
    }

    return result;
}

// Read a wave file and return the sampled audio. The file must
// be 44100 kHz and PCM. If it is multiple channels, only the 
// first will be returned.
//
vector<int16_t> readWave(const string &fname)
{
    vector<char> data = readFile(fname);

    // offsets of important header bits
    //
    const int OFF_RIFF = 0;
    const int OFF_RIFFSIZE = 4;
    const int OFF_WAVE = 8;
    const int OFF_CHUNKDATA = 12;

    if (
        data.size() < OFF_CHUNKDATA || 
        !checkFourCC(data, OFF_RIFF, "RIFF") || 
        !checkFourCC(data, OFF_WAVE, "WAVE") 
    ) {
        throw runtime_error{ "not a wave file" };
    }

    // Walk the RIFF structure and find the format and data chunks
    //
    int waveChunkSize = extract32(data, OFF_RIFFSIZE) - sizeof(uint32_t);
    int at = OFF_CHUNKDATA;

    int fmtAt = -1;
    int fmtLen = 0;
    int dataAt = -1;
    int dataLen = 0;

    while (at < waveChunkSize) {
        if (checkFourCC(data, at, "fmt ")) {
            fmtAt = at + 2 * sizeof(uint32_t);
            fmtLen = extract32(data, at + sizeof(uint32_t));
        }

        if (checkFourCC(data, at, "data")) {
            dataAt = at + 2 * sizeof(uint32_t);
            dataLen = extract32(data, at + sizeof(uint32_t));
        }

        at += 2 * sizeof(uint32_t) + extract32(data, at + sizeof(uint32_t));
    }

    // Verify supported format
    //
    const int OFF_FORMAT_TAG = 0;
    const int OFF_CHANNELS = 2;
    const int OFF_SAMPLE_RATE = 4;
    const int OFF_BIT_DEPTH = 14;
    const int FMT_MIN_SIZE = 16;


    if (fmtAt == -1 || dataAt == -1 || fmtLen < FMT_MIN_SIZE) {
        throw runtime_error{ "Wave file is corrupt (does not contain format and/or data)." };
    }

    int format = extract16(data, fmtAt + OFF_FORMAT_TAG);
    int channels = extract16(data, fmtAt + OFF_CHANNELS);
    int sampleRate = extract32(data, fmtAt + OFF_SAMPLE_RATE);
    int bitDepth = extract16(data, fmtAt + OFF_BIT_DEPTH);

    if (
        format != 1 ||
        (channels != 1 && channels != 2) ||
        sampleRate != 44100 ||
        bitDepth != 16
    ) {
        throw runtime_error{ "Wave file must be 44Khz mono/stereo PCM." };
    }

    vector<int16_t> waveData;
    int stride = (channels == 1) ? 2 : 4;

    int end = dataAt + dataLen;
    for (; dataAt < end; dataAt += stride) {
        waveData.push_back(static_cast<int16_t>(extract16(data, dataAt)));
    }

    return waveData;
}

void put16(ofstream &out, int val) 
{
    for (int i = 0; i < 2; i++) {
        char ch = val & 0xff;
        out.write(&ch, 1);
        val >>= 8;
    }
}

void put32(ofstream &out, int val) 
{
    for (int i = 0; i < 4; i++) {
        char ch = val & 0xff;
        out.write(&ch, 1);
        val >>= 8;
    }
}


void writeWave(const std::string &fname, const std::vector<int16_t> &data)
{
    ofstream out{ fname, ios::out | ios::binary };
    if (!out) {
        throw runtime_error{ "Could not open output file." };
    }

    int dataBytes = data.size() * 2;
    int fmtSize = 16;

    int riffSize = 
        4 +         // WAVE
        4 +         // fmt 
        4 +         // fmt chunk size
        fmtSize +
        4 +         // data
        4 +         // data chunk size
        dataBytes;

    out << "RIFF";
    put32(out, riffSize);
    out << "WAVEfmt ";
    put32(out, fmtSize);

    // see: WAVEFORMATEX from Win32
    put16(out, PCM_TAG);        // wFormatTag
    put16(out, 1);              // nChannels
    put32(out, 44100);          // nSamplesPerSec
    put32(out, 2*44100);        // nAvgBytesPerSec
    put16(out, 2);              // nBlockAlign
    put16(out, 16);             // nBitsPerSample

    out << "data";
    put32(out, dataBytes);

    for (int val : data) {
        put16(out, val);
    }
}
