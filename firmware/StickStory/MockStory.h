#pragma once

#include "Story.h"

namespace mock {

enum SceneId { Intro, Pick, ResultA, ResultB };
enum Background : story::AssetId { BgTest = 1 };
enum Portrait : story::AssetId { TestA = 1, TestB };
enum Sound : story::AssetId { Positive = 1, Negative };

inline constexpr story::Option Options[] = {
    {"测试选项 A", ResultA, 10},
    {"测试选项 B", ResultB, -10},
};

inline constexpr story::Scene Scenes[] = {
    {story::Kind::Dialogue, "测试角色 A", "这是一条测试对白。按 A 进入选项。", BgTest, TestA,
     story::NoAsset, Pick, nullptr, 0},
    {story::Kind::Choice, "测试角色 A", "请选择一个测试分支。", BgTest, TestA,
     story::NoAsset, story::NoScene, Options, sizeof(Options) / sizeof(Options[0])},
    {story::Kind::Ending, "测试角色 A", "分支 A 完成。好感度增加 10。", BgTest, TestA,
     Positive, story::NoScene, nullptr, 0},
    {story::Kind::Ending, "测试角色 B", "分支 B 完成。好感度减少 10。", BgTest, TestB,
     Negative, story::NoScene, nullptr, 0},
};

inline constexpr size_t SceneCount = sizeof(Scenes) / sizeof(Scenes[0]);

}  // namespace mock
