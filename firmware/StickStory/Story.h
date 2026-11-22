#pragma once

#include <stddef.h>
#include <stdint.h>

namespace story {

enum class Kind { Dialogue, Choice, Ending };
using AssetId = uint16_t;
constexpr AssetId NoAsset = 0;
constexpr size_t NoScene = static_cast<size_t>(-1);

struct Option {
    const char* label;
    size_t next;
    int8_t affectionDelta;
};

struct Scene {
    Kind kind;
    const char* speaker;
    const char* text;
    AssetId background;
    AssetId portrait;
    AssetId sound;
    size_t next;
    const Option* options;
    size_t optionCount;
};

class Player {
public:
    Player(const Scene* scenes, size_t sceneCount);

    const Scene& current() const { return scenes_[scene_]; }
    size_t sceneIndex() const { return scene_; }
    size_t sceneCount() const { return sceneCount_; }
    size_t selectedOption() const { return selectedOption_; }
    int affection() const { return affection_; }

    void reset();
    bool advance();
    bool moveSelection();
    bool confirm();
    bool jumpTo(size_t next);
    bool restore(size_t scene, int affection);

private:
    bool enter(size_t next);

    const Scene* scenes_;
    size_t sceneCount_;
    size_t scene_ = 0;
    size_t selectedOption_ = 0;
    int affection_ = 50;
};

}  // namespace story
