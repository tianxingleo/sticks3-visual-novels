#pragma once

#include <stddef.h>
#include <stdint.h>

namespace yuiline {

enum class Reply { Ignore, Acknowledged, Complete, Failed };

class Continuation {
public:
    static constexpr size_t MaxLines = 12;
    static constexpr size_t OptionCount = 2;

    void reset(uint32_t session, uint16_t turn, uint32_t attempt);
    Reply receive(const char* message);

    size_t lineCount = 0;
    char lines[MaxLines][38 * 4 + 1] = {};
    char options[OptionCount][14 * 4 + 1] = {};
    char error[97] = {};

private:
    Reply fail(const char* reason);

    uint32_t session_ = 0;
    uint32_t attempt_ = 0;
    uint16_t turn_ = 0;
    size_t expectedLines_ = 0;
    size_t optionsReceived_ = 0;
    bool receiving_ = false;
};

}  // namespace yuiline
