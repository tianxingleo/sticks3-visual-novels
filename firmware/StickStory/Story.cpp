#include "Story.h"

namespace story {

Player::Player(const Scene* scenes, size_t sceneCount)
    : scenes_(scenes), sceneCount_(sceneCount) {}

void Player::reset() {
    scene_ = 0;
    selectedOption_ = 0;
    affection_ = 50;
}

bool Player::enter(size_t next) {
    if (next >= sceneCount_) return false;
    scene_ = next;
    selectedOption_ = 0;
    return true;
}

bool Player::advance() {
    switch (current().kind) {
        case Kind::Dialogue:
            return enter(current().next);
        case Kind::Choice:
            return false;
        case Kind::Ending:
            reset();
            return true;
    }
    return false;
}

bool Player::moveSelection() {
    const Scene& scene = current();
    if (scene.kind != Kind::Choice || scene.optionCount < 2) return false;
    selectedOption_ = (selectedOption_ + 1) % scene.optionCount;
    return true;
}

bool Player::confirm() {
    const Scene& scene = current();
    if (scene.kind != Kind::Choice || selectedOption_ >= scene.optionCount) return false;

    const Option& option = scene.options[selectedOption_];
    if (!enter(option.next)) return false;

    affection_ += option.affectionDelta;
    if (affection_ < 0) affection_ = 0;
    if (affection_ > 100) affection_ = 100;
    return true;
}

bool Player::jumpTo(size_t next) { return enter(next); }

bool Player::restore(size_t scene, int affection) {
    if (scene >= sceneCount_) return false;
    scene_ = scene;
    selectedOption_ = 0;
    affection_ = affection;
    if (affection_ < 0) affection_ = 0;
    if (affection_ > 100) affection_ = 100;
    return true;
}

}  // namespace story
