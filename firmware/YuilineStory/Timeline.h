#pragma once

#include <stddef.h>

namespace yuiline {

enum class Portrait { Hidden, Normal, Sad };
enum class Sound { None, Crowd, Rice, Heels, Water };

struct Beat {
    bool night;
    Portrait portrait;
    Sound sound;
};

// Indices follow game.html's zero-based triggers. Heels follows the arrival in the dialogue.
constexpr Beat beatAt(size_t line) {
    Portrait portrait = Portrait::Hidden;
    if (line >= 17 && line < 34) portrait = Portrait::Normal;
    if (line >= 35 && line < 47) portrait = Portrait::Sad;
    Sound sound = Sound::None;
    switch (line) {
        case 1: sound = Sound::Crowd; break;
        case 2:
        case 3: sound = Sound::Rice; break;
        case 12: sound = Sound::Heels; break;
        case 73: sound = Sound::Water; break;
        default: break;
    }
    return {line >= 56, portrait, sound};
}

}  // namespace yuiline
