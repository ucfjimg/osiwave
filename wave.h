#ifndef WAVE_H
#define WAVE_H

#include <string>
#include <vector>

extern std::vector<int16_t> readWave(const std::string &fname);
extern void writeWave(const std::string &fname, const std::vector<int16_t> &data); 

#endif
