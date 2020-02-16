#ifndef WAVE_H
#define WAVE_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

class WaveReader {
public:
    WaveReader(const std::string &fname);

    int getSampleRate() const { return sampleRate_; }
    int getChannels() const { return nchannels_; }

    void setReadChannel(int chan);
    std::vector<int16_t> readSamples(uint32_t nsamples);

private:
    int sampleRate_;
    int nchannels_;
    int readChan_;
    std::ifstream in_;
    uint32_t endOfData_;
    std::vector<char> readBuf_;

    std::string readFourCC();
    uint32_t readUnsignedWord(int nbytes);
};

#endif
