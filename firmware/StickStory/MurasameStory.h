#pragma once

#include "Story.h"

// 原创迷你剧情，结构参考纯前端互动小游戏的好感度分支思想。
// 对白和数值均为本工程原创，不包含原作图片、音频和原文。
namespace murasame {

enum SceneId { Intro = 0, FirstTouch = 1, HappyLoop = 2, AlertTalk = 3, AlertChoice = 4, Win = 5, Fail = 6, Easter = 7 };
enum Background : story::AssetId { Shrine = 1, Room = 2, Night = 3 };
enum Portrait : story::AssetId { Normal = 1, Happy = 2, Upset = 3 };
enum Sound : story::AssetId { Positive = 1, Negative = 2, Select = 3, WinCue = 4, FailCue = 5, EasterCue = 6 };

inline constexpr story::Option FirstOptions[] = {
    {"轻轻摸头", HappyLoop, 8},
    {"收回手观察", AlertTalk, -5},
};

inline constexpr story::Option HappyOptions[] = {
    {"继续轻摸", HappyLoop, 10},
    {"停下问感受", AlertTalk, -10},
};

inline constexpr story::Option AlertOptions[] = {
    {"道歉重来", FirstTouch, -10},
    {"轻声安抚", HappyLoop, 5},
    {"转身离开", Fail, -20},
    {"讲个冷笑话", Easter, 0},
};

inline constexpr story::Scene Scenes[] = {
    {story::Kind::Dialogue, "旁白", "风穿过神社的石灯笼。她抬头看你。", Shrine, Normal,
     story::NoAsset, FirstTouch, nullptr, 0},
    {story::Kind::Choice, "丛雨", "要做什么？先说好，摸头要负责。", Shrine, Normal,
     story::NoAsset, story::NoScene, FirstOptions, sizeof(FirstOptions) / sizeof(FirstOptions[0])},
    {story::Kind::Choice, "丛雨", "唔…手法还不赖。再来一下也行。", Shrine, Happy,
     Positive, story::NoScene, HappyOptions, sizeof(HappyOptions) / sizeof(HappyOptions[0])},
    {story::Kind::Dialogue, "旁白", "她后退半步，抱紧短刀。灯晃了一下。", Room, Upset,
     Negative, AlertChoice, nullptr, 0},
    {story::Kind::Choice, "丛雨", "你犹豫了？说吧，怎么补偿我？", Room, Upset,
     story::NoAsset, story::NoScene, AlertOptions, sizeof(AlertOptions) / sizeof(AlertOptions[0])},
    {story::Kind::Ending, "丛雨", "好感度满了！月亮分你一半吧。", Night, Happy,
     WinCue, story::NoScene, nullptr, 0},
    {story::Kind::Ending, "丛雨", "修行失败！下次用刀背敲你。", Night, Upset,
     FailCue, story::NoScene, nullptr, 0},
    {story::Kind::Ending, "旁白", "你被骗啦！这只是彩蛋结局。", Shrine, Happy,
     EasterCue, story::NoScene, nullptr, 0},
};

inline constexpr size_t SceneCount = sizeof(Scenes) / sizeof(Scenes[0]);

}  // namespace murasame
