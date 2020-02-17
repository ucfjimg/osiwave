#ifndef FRAMEFLT_H
#define FRAMEFLT_H

#include <array>
#include <vector>

class BitstreamFilter;

class FrameFilter {
public:
    FrameFilter(BitstreamFilter &bs);

    std::vector<char> getChars(int nchars);

private:
    const int WINDOW = 1024;

    static const int FRAME = 11;
    
    BitstreamFilter &bs_;
    std::vector<bool> bits_;
    int bitIdx_;
    bool eof_;

    std::array<bool, FRAME> ring_;
    int ringBase_;

    bool frameAt(int idx) const;
    void frameShift();
    void refillFrame();

    bool getNextBit();
};

#endif
