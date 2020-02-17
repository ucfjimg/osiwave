#include "frameflt.h"

#include "bitstrm.h"

#include <vector>

using std::vector;

FrameFilter::FrameFilter(BitstreamFilter &bs)
    : bs_(bs)
    , bitIdx_(0)
    , eof_(false)
    , ringBase_(0)
{
    bits_ = bs.getBits(WINDOW);

    for (int i = 0; i < FRAME; i++) {
        ring_[i] = getNextBit();
    }   
}

// Return some decoded character data
vector<char> FrameFilter::getChars(int nchars)
{
    vector<char> chars;

    while (chars.size() < nchars && !eof_) {
        // frame format is
        //           1
        // 01234567890
        // MSXXXXXXXXM  where M is leading marks, S is the start bit (a space), and
        // the last M is the stop bit. The X's are the byte, LSB first.
        //      
        if (frameAt(0) && !frameAt(1) && frameAt(10)) {
            char ch = 0;
            for (int b = 0; b < 8; b++) {
                if (frameAt(2 + b)) {
                    ch |= (1 << b);
                }
            }

            // kind of hacky, we are specifically looking for ASCII data so
            // throw away false positives based on the encoding.
            if (ch == '\r' || ch == '\n' || ch == '\0' || (ch >= 0x20 && ch <= 0x7e)) {
                chars.push_back(ch);
                refillFrame();
                continue;
            }
        }

        frameShift();
        continue;
    }

    return chars;
}

// return the bit at position `idx' in the candidate frame
bool FrameFilter::frameAt(int idx) const
{
    // the frame is kept in a ring buffer to prevent lots of shifting.
    return ring_[(ringBase_ + idx) % FRAME];
}

// shift one new bit in the frame buffer
void FrameFilter::frameShift()
{
    ring_[ringBase_] = getNextBit();
    ringBase_ = (ringBase_ + 1) % FRAME;
}

// refill the entire frame except for the MARK at the end
void FrameFilter::refillFrame()
{
    ring_[0] = ring_.back();
    for (int i = 1; i < FRAME; i++) {
        ring_[i] = getNextBit();
    }
    ringBase_ = 0;
}

// Get the next buffered bit
bool FrameFilter::getNextBit()
{
    if (eof_) {
        return false;
    }

    if (bitIdx_ < bits_.size()) {
        return bits_[bitIdx_++];
    }

    bits_ = bs_.getBits(WINDOW);
    bitIdx_ = 0;

    if (bits_.size() == 0) {
        eof_ = true;
        return false;
    }

    return getNextBit();
}
