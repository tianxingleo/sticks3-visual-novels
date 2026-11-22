#pragma once

#include <stddef.h>
#include <stdint.h>

namespace yuiline {

struct Blob {
    const uint8_t* data;
    size_t size;
};

extern const char* const Lines[];
extern const size_t LineCount;
extern const Blob Day, Night, Normal1, Normal2, Normal3, Sad1, Sad2, Sad3, Back;
extern const Blob Jazz, Crowd, Rice, Heels, Water;
constexpr uint32_t SampleRate = 12000;

}  // namespace yuiline
