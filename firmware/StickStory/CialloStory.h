#pragma once

#include "Story.h"

// 完整移植纯前端互动小游戏的全部分支（含隐藏结局与彩蛋）。
// 流程与短句来自粉丝同人作品的玩法逻辑；原作图片与语音不在此仓库中，
// 通过 tools/make_assets.py 由用户在本地自行放入素材后生成 Assets.h。
// 部分生僻字与表情符号按设备字库做了替换，原句保留在注释中。
namespace ciallo {

enum SceneId { Step1 = 0, Step2 = 1, Step22 = 2, Step213 = 3, Win = 4, Fail = 5, Easter = 6 };
enum Photo : story::AssetId { PhotoCiallo = 1, PhotoM1 = 2, PhotoM2 = 3, PhotoM3 = 4, PhotoWin = 5, PhotoFail = 6 };
enum Portrait : story::AssetId { Normal = 1, Happy = 2, Upset = 3 };
enum Sound : story::AssetId {
    Start = 1,
    Voice1 = 2,
    Voice2 = 3,
    Voice3 = 4,
    Voice4 = 5,
    Voice5 = 6,
    Lemon = 7,
    CialloEgg = 8,
    Select = 9
};

inline constexpr story::Option Step1Options[] = {
    {"开始摸头", Step2, 0},
};

inline constexpr story::Option Step2Options[] = {
    {"嘿嘿，幼刀酱~继续摸", Step213, 8},
    {"我好像踩到地雷了，寸止！", Step22, -5},
};

inline constexpr story::Option Step22Options[] = {
    // 原句：吓我一跳,释放时间回溯忍术！
    {"吓我一跳！时间回溯！", Step1, -10},
    // 原句：幼刀酱是个纯小子，那只能摸头了（+表情）
    {"纯小子只能摸头了", Step213, 5},
    {"区区太平公主，我避她锋芒？", Fail, -20},
    {"666还有互动环节", Easter, 0},
};

inline constexpr story::Option Step213Options[] = {
    {"无限摸头摸个爽！", Step213, 10},
    // 原句：不摸了,她总不能是滑动变阻器吧
    {"不摸了！滑动变阻器吧", Step22, -10},
};

inline constexpr story::Option WinOptions[] = {
    {"666还有第二关", Step1, 0},  // 由主程序特殊处理：好感度重置为 50
    {"退出游戏", Easter, 0},      // 由主程序特殊处理：好感度设为 78
};

inline constexpr story::Option FailOptions[] = {
    {"第三炸弹，败者食尘！", Step1, 0},  // 由主程序特殊处理：好感度重置为 50
    {"退出游戏", Easter, 0},            // 由主程序特殊处理：好感度设为 78
};

inline constexpr story::Option EasterOptions[] = {
    {"战败BGM", Easter, 0},  // 自循环，每次点都重播 Lemon 提示音
};

inline constexpr story::Scene Scenes[] = {
    // 原配文：雑魚柚子厨~（+表情）
    {story::Kind::Choice, "丛雨", "小柚子厨~", PhotoCiallo, Happy, Start, story::NoScene, Step1Options,
     sizeof(Step1Options) / sizeof(Step1Options[0])},
    {story::Kind::Choice, "丛雨", "我去，摸头起手？你高低是个三千！", PhotoM1, Normal, Voice1,
     story::NoScene, Step2Options, sizeof(Step2Options) / sizeof(Step2Options[0])},
    {story::Kind::Choice, "丛雨", "狗！修！金！！！", PhotoM2, Upset, Voice2, story::NoScene,
     Step22Options, sizeof(Step22Options) / sizeof(Step22Options[0])},
    // 原配文：狗~修~金~（+表情）
    {story::Kind::Choice, "丛雨", "狗~修~金~", PhotoM3, Happy, Voice3, story::NoScene,
     Step213Options, sizeof(Step213Options) / sizeof(Step213Options[0])},
    // 原配文：嘿嘿~ 狗修金（+表情）
    {story::Kind::Choice, "丛雨", "嘿嘿~狗修金", PhotoWin, Happy, Voice5, story::NoScene,
     WinOptions, sizeof(WinOptions) / sizeof(WinOptions[0])},
    // 原配文：豪赤（+表情）！！！
    {story::Kind::Choice, "丛雨", "豪赤！！！", PhotoFail, Upset, Voice4, story::NoScene,
     FailOptions, sizeof(FailOptions) / sizeof(FailOptions[0])},
    // 原配文：你 被 骗 了 ! ! !
    {story::Kind::Choice, "旁白", "你被骗了！！！", PhotoCiallo, Happy, Lemon, story::NoScene,
     EasterOptions, sizeof(EasterOptions) / sizeof(EasterOptions[0])},
};

inline constexpr size_t SceneCount = sizeof(Scenes) / sizeof(Scenes[0]);

}  // namespace ciallo
