#include <M5Unified.h>
#include <Preferences.h>

#include "CialloStory.h"
#include "Story.h"

#if __has_include("Assets.h")
#include "Assets.h"
#define HAS_BUNDLED_ASSETS 1
#else
#define HAS_BUNDLED_ASSETS 0
#endif

namespace {

using story::Kind;

story::Player player(ciallo::Scenes, ciallo::SceneCount);
M5Canvas frame(&M5.Display);
Preferences prefs;

enum class AppMode { Title, Story };
AppMode mode = AppMode::Title;

constexpr uint16_t PanelDark = 0x0E25;
constexpr uint16_t Border = 0x3B0E;
constexpr uint16_t Text = 0xF7BE;
constexpr uint16_t Muted = 0xAE15;
constexpr uint16_t Highlight = 0x7F59;
constexpr uint16_t AccentPink = 0xFCAB;
constexpr uint16_t BarBack = 0x2A69;
constexpr uint16_t BarFill = 0xF8A7;
constexpr int Margin = 8;
constexpr uint32_t CharMs = 55;
constexpr uint32_t BgmMs = 340;
constexpr uint32_t EggHoldMs = 700;
constexpr uint32_t TitleHoldMs = 1000;

enum FallbackBg : story::AssetId { FbShrine = 1, FbRoom = 2, FbNight = 3 };

uint32_t sceneStarted = 0;
uint32_t lastBgm = 0;
size_t bgmStep = 0;
bool typewriterDone = false;
bool hasSave = false;
int savedScene = -1;
int savedAffection = 50;
uint32_t toastUntil = 0;
size_t eggTone = 0;
bool photoShown = false;

// Button hold tracking.
bool aDown = false;
uint32_t aDownAt = 0;
bool aTitleFired = false;
bool bDown = false;
uint32_t bDownAt = 0;
bool bEggFired = false;

size_t utf8Chars(const char* text) {
    size_t count = 0;
    while (*text != '\0') {
        ++text;
        while ((static_cast<uint8_t>(*text) & 0xC0) == 0x80) ++text;
        ++count;
    }
    return count;
}

String utf8Head(const char* text, size_t chars) {
    String out;
    size_t count = 0;
    while (*text != '\0' && count < chars) {
        const char* start = text++;
        while ((static_cast<uint8_t>(*text) & 0xC0) == 0x80) ++text;
        out.concat(start, static_cast<unsigned int>(text - start));
        ++count;
    }
    return out;
}

size_t visibleChars(uint32_t now) {
    if (typewriterDone) return utf8Chars(player.current().text);
    const uint32_t elapsed = now - sceneStarted;
    return 1 + elapsed / CharMs;
}

void drawProcedural(FallbackBg bg) {
    const int w = frame.width();
    const int h = frame.height();
    if (bg == FbNight) {
        for (int y = 0; y < h; y += 3) {
            const int depth = y / 3;
            frame.fillRect(0, y, w, 3, frame.color565(10 + depth / 6, 18 + depth / 4, 48 + depth / 2));
        }
        frame.fillCircle(198, 34, 13, frame.color565(240, 230, 180));
        frame.fillCircle(194, 31, 11, frame.color565(22, 32, 72));
        for (int i = 0; i < 26; ++i) {
            const int x = (i * 47 + 11) % (w - 4) + 2;
            const int y = (i * 31 + 7) % 80 + 4;
            frame.drawPixel(x, y, frame.color565(180, 200, 235));
        }
        frame.fillRect(0, 108, w, h - 108, frame.color565(16, 26, 52));
        return;
    }
    if (bg == FbRoom) {
        frame.fillScreen(frame.color565(74, 52, 40));
        frame.fillRect(0, 0, w, 70, frame.color565(96, 68, 50));
        frame.fillRect(0, 70, w, 4, frame.color565(52, 34, 26));
        frame.fillRect(0, 74, w, h - 74, frame.color565(62, 42, 32));
        frame.fillCircle(36, 46, 14, frame.color565(255, 214, 150));
        frame.fillCircle(36, 46, 10, frame.color565(255, 236, 200));
        frame.drawFastVLine(120, 8, 54, frame.color565(70, 48, 36));
        frame.drawFastVLine(180, 8, 54, frame.color565(70, 48, 36));
        return;
    }
    // Shrine (default).
    for (int y = 0; y < h; y += 3) {
        const int depth = y / 3;
        frame.fillRect(0, y, w, 3, frame.color565(120 + depth, 150 + depth / 2, 200 - depth / 3));
    }
    frame.fillCircle(52, 44, 16, frame.color565(255, 250, 235));
    frame.fillRect(0, 96, w, h - 96, frame.color565(86, 132, 96));
    frame.fillRect(150, 30, 8, 70, frame.color565(178, 44, 40));
    frame.fillRect(196, 30, 8, 70, frame.color565(178, 44, 40));
    frame.fillRect(140, 22, 74, 10, frame.color565(198, 56, 48));
    frame.fillRect(146, 36, 62, 5, frame.color565(160, 38, 36));
    const uint32_t now = millis();
    for (int i = 0; i < 8; ++i) {
        const int x = (i * 41 + 20 + int(now / (90 + i * 13)) * 2) % w;
        const int y = 10 + (60 + i * 17 + int(now / (140 + i * 11) % 80)) % 86;
        frame.fillCircle(x, y, 2, frame.color565(255, 200, 214));
    }
}

FallbackBg fallbackFor(story::AssetId photo) {
    if (photo == ciallo::PhotoM2) return FbRoom;
    if (photo == ciallo::PhotoWin || photo == ciallo::PhotoFail) return FbNight;
    return FbShrine;
}

bool drawBundledPhoto(story::AssetId photo) {
#if HAS_BUNDLED_ASSETS
    const uint8_t* data = nullptr;
    size_t len = 0;
    switch (photo) {
        case ciallo::PhotoCiallo: data = assets::IMG_CIALLO; len = assets::IMG_CIALLO_LEN; break;
        case ciallo::PhotoM1: data = assets::IMG_M1; len = assets::IMG_M1_LEN; break;
        case ciallo::PhotoM2: data = assets::IMG_M2; len = assets::IMG_M2_LEN; break;
        case ciallo::PhotoM3: data = assets::IMG_M3; len = assets::IMG_M3_LEN; break;
        case ciallo::PhotoWin: data = assets::IMG_WIN; len = assets::IMG_WIN_LEN; break;
        case ciallo::PhotoFail: data = assets::IMG_FAIL; len = assets::IMG_FAIL_LEN; break;
        default: break;
    }
    if (data != nullptr && len > 0) {
        return frame.drawJpg(data, static_cast<uint32_t>(len), 0, 0);
    }
#else
    (void)photo;
#endif
    return false;
}

void drawSceneBackground(story::AssetId photo) {
    photoShown = drawBundledPhoto(photo);
    if (!photoShown) drawProcedural(fallbackFor(photo));
}

void drawPortrait(story::AssetId portrait, int x, int y) {
    if (portrait == story::NoAsset) return;
    constexpr int W = 52;
    constexpr int H = 48;
    frame.fillRoundRect(x, y, W, H, 6, PanelDark);
    frame.drawRoundRect(x, y, W, H, 6, Border);
    const int cx = x + W / 2;
    const int cy = y + H / 2 + 2;
    frame.fillCircle(cx, cy - 4, 17, frame.color565(52, 68, 120));
    frame.fillRect(cx - 17, cy - 4, 34, 12, frame.color565(52, 68, 120));
    frame.fillCircle(cx, cy, 12, frame.color565(255, 224, 196));
    frame.fillTriangle(cx - 17, cy - 8, cx - 4, cy - 14, cx - 6, cy - 2, frame.color565(52, 68, 120));
    frame.fillTriangle(cx + 17, cy - 8, cx + 4, cy - 14, cx + 6, cy - 2, frame.color565(52, 68, 120));
    frame.fillTriangle(cx - 14, cy - 14, cx - 8, cy - 24, cx - 4, cy - 13, frame.color565(52, 68, 120));
    frame.fillTriangle(cx + 14, cy - 14, cx + 8, cy - 24, cx + 4, cy - 13, frame.color565(52, 68, 120));

    if (portrait == ciallo::Happy) {
        frame.drawArc(cx - 6, cy, 4, 4, 200, 340, frame.color565(30, 20, 20));
        frame.drawArc(cx + 6, cy, 4, 4, 200, 340, frame.color565(30, 20, 20));
        frame.fillCircle(cx - 9, cy + 5, 2, frame.color565(255, 150, 160));
        frame.fillCircle(cx + 9, cy + 5, 2, frame.color565(255, 150, 160));
        frame.fillCircle(cx, cy + 6, 3, frame.color565(150, 60, 60));
    } else if (portrait == ciallo::Upset) {
        frame.drawLine(cx - 10, cy - 4, cx - 3, cy - 1, frame.color565(30, 20, 20));
        frame.drawLine(cx + 10, cy - 4, cx + 3, cy - 1, frame.color565(30, 20, 20));
        frame.fillCircle(cx - 6, cy + 2, 2, frame.color565(30, 20, 20));
        frame.fillCircle(cx + 6, cy + 2, 2, frame.color565(30, 20, 20));
        frame.drawLine(cx - 4, cy + 8, cx + 4, cy + 6, frame.color565(30, 20, 20));
        frame.drawLine(cx + 12, cy - 12, cx + 16, cy - 8, AccentPink);
        frame.drawLine(cx + 16, cy - 12, cx + 12, cy - 8, AccentPink);
    } else {
        frame.fillCircle(cx - 6, cy + 1, 2, frame.color565(30, 20, 20));
        frame.fillCircle(cx + 6, cy + 1, 2, frame.color565(30, 20, 20));
        frame.drawLine(cx - 3, cy + 7, cx + 3, cy + 7, frame.color565(30, 20, 20));
    }
}

void drawWrapped(const String& text, int x, int y, int maxWidth) {
    String line;
    int row = 0;
    size_t i = 0;
    while (i < text.length() && row < 2) {
        size_t next = i + 1;
        while (next < text.length() && (static_cast<uint8_t>(text[next]) & 0xC0) == 0x80) ++next;
        const String piece = text.substring(i, next);
        const String trial = line + piece;
        if (!line.isEmpty() && frame.textWidth(trial) > maxWidth) {
            frame.drawString(line, x, y + row * 13);
            line = piece;
            ++row;
            if (row == 2) return;
        } else {
            line = trial;
        }
        i = next;
    }
    if (!line.isEmpty() && row < 2) frame.drawString(line, x, y + row * 13);
}

void drawTopBar() {
    const int w = frame.width();
    frame.fillRect(0, 0, w, 26, PanelDark);
    frame.drawFastHLine(0, 26, w, Border);
    frame.setFont(&fonts::efontCN_12);
    frame.setTextDatum(top_left);
    frame.setTextColor(Muted);
    frame.drawString("丛雨摸头修行", Margin, 4);

    char score[24];
    snprintf(score, sizeof(score), "好感 %d", player.affection());
    frame.setTextDatum(top_right);
    frame.setTextColor(Text);
    frame.drawString(score, w - Margin, 4);
    frame.setTextDatum(top_left);

    const int barX = Margin;
    const int barY = 18;
    const int barW = w - 2 * Margin;
    frame.drawRect(barX, barY, barW, 5, Border);
    const int fill = player.affection() * (barW - 2) / 100;
    frame.fillRect(barX + 1, barY + 1, barW - 2, 3, BarBack);
    if (fill > 0) frame.fillRect(barX + 1, barY + 1, fill, 3, BarFill);
}

bool portraitVisible() {
    return !photoShown && player.current().portrait != story::NoAsset;
}

void renderStory(uint32_t now) {
    const auto& scene = player.current();
    const int w = frame.width();
    const int h = frame.height();

    drawSceneBackground(scene.background);
    drawTopBar();

    const bool showPortrait = portraitVisible();
    if (showPortrait) drawPortrait(scene.portrait, Margin, 30);
    const int textX = showPortrait ? 68 : Margin;
    frame.setFont(&fonts::efontCN_12);
    frame.setTextDatum(top_left);
    frame.fillRoundRect(textX - 4, 30, frame.textWidth(scene.speaker) + 12, 16, 4, PanelDark);
    frame.setTextColor(Highlight);
    frame.drawString(scene.speaker, textX + 2, 31);

    frame.fillRoundRect(Margin, 82, w - 2 * Margin, 34, 5, PanelDark);
    frame.drawRoundRect(Margin, 82, w - 2 * Margin, 34, 5, Border);
    frame.setTextColor(Text);
    const String shown = utf8Head(scene.text, visibleChars(now));
    drawWrapped(shown, Margin + 6, 86, w - 2 * Margin - 12);

    if (scene.kind == Kind::Choice && scene.optionCount > 0) {
        constexpr size_t VisibleRows = 2;
        const size_t firstVisible = player.selectedOption() / VisibleRows * VisibleRows;
        for (size_t row = 0; row < VisibleRows && firstVisible + row < scene.optionCount; ++row) {
            const size_t index = firstVisible + row;
            const bool selected = index == player.selectedOption();
            const int y = 48 + static_cast<int>(row) * 17;
            const int optW = showPortrait ? w - Margin - 68 : w - 2 * Margin;
            const int optX = showPortrait ? 68 : Margin;
            frame.fillRoundRect(optX, y, optW, 15, 4, selected ? Highlight : PanelDark);
            frame.setTextColor(selected ? PanelDark : Text);
            frame.drawString(scene.options[index].label, optX + 6, y + 2);
        }
        if (scene.optionCount > VisibleRows) {
            frame.setTextColor(Muted);
            frame.drawString("…", w - Margin - 10, 48);
        }
    }

    frame.setTextColor(Muted);
    const char* hint = "A 继续";
    if (scene.kind == Kind::Choice) hint = scene.optionCount < 2 ? "B 确认" : "A 切换  B 确认";
    if (scene.kind == Kind::Ending) hint = "A 回标题";
    frame.drawString(hint, Margin, h - 15);

    if (typewriterDone && ((now / 500) % 2 == 0)) {
        frame.fillTriangle(w - Margin - 6, h - 8, w - Margin, h - 8, w - Margin - 3, h - 4, Highlight);
    }
    if (toastUntil != 0 && now < toastUntil) {
        frame.setFont(&fonts::efontCN_14);
        frame.setTextDatum(top_left);
        const char* egg = "ciallo~";
        const int tw = frame.textWidth(egg) + 24;
        const int tx = (w - tw) / 2;
        frame.fillRoundRect(tx, 52, tw, 26, 8, PanelDark);
        frame.drawRoundRect(tx, 52, tw, 26, 8, AccentPink);
        frame.setTextColor(AccentPink);
        frame.drawString(egg, tx + 12, 58);
    }
    frame.pushSprite(0, 0);
}

void renderTitle(uint32_t now) {
    const int w = frame.width();
    const int h = frame.height();
    photoShown = drawBundledPhoto(ciallo::PhotoCiallo);
    if (!photoShown) drawProcedural(FbShrine);
    frame.fillRect(0, 0, w, 30, PanelDark);
    frame.drawFastHLine(0, 30, w, Border);
    frame.setFont(&fonts::efontCN_14);
    frame.setTextDatum(top_left);
    frame.setTextColor(Text);
    frame.drawString("丛雨摸头修行", Margin, 4);
    frame.setFont(&fonts::efontCN_12);
    frame.setTextColor(Muted);
    frame.drawString("StickS3 完整复刻", Margin, 26 - 6);

    if (!photoShown) drawPortrait(ciallo::Happy, w - Margin - 52, 38);

    frame.fillRoundRect(Margin, 38, 150, 66, 6, PanelDark);
    frame.drawRoundRect(Margin, 38, 150, 66, 6, Border);
    frame.setTextColor(Text);
    frame.drawString("A 新游戏", Margin + 8, 42);
    if (hasSave) {
        char save[32];
        snprintf(save, sizeof(save), "B 继续·好感%d", savedAffection);
        frame.drawString(save, Margin + 8, 58);
    } else {
        frame.setTextColor(Muted);
        frame.drawString("暂无存档", Margin + 8, 58);
    }
    frame.setTextColor(Muted);
    frame.drawString("好感50开局", Margin + 8, 74);
    frame.drawString("长按B彩蛋 A回标题", Margin + 8, 88);

    frame.setTextColor(Muted);
    frame.drawString("选项:A切 B定", Margin, h - 15);
    if ((now / 500) % 2 == 0) {
        frame.setTextColor(Highlight);
        frame.drawString("按 A 开始", w - Margin - 56, h - 15);
    }
    frame.pushSprite(0, 0);
}

void playToneOrWav(const uint8_t* wav, size_t len, uint16_t freq, uint32_t ms) {
#if HAS_BUNDLED_ASSETS
    if (wav != nullptr && len > 0) {
        M5.Speaker.playWav(wav, len);
        return;
    }
#else
    (void)wav;
    (void)len;
#endif
    M5.Speaker.tone(freq, ms);
}

void playCue(story::AssetId sound) {
#if HAS_BUNDLED_ASSETS
    const uint8_t* wav = nullptr;
    size_t len = 0;
    switch (sound) {
        case ciallo::Voice1: wav = assets::WAV_VOICE1; len = assets::WAV_VOICE1_LEN; break;
        case ciallo::Voice2: wav = assets::WAV_VOICE2; len = assets::WAV_VOICE2_LEN; break;
        case ciallo::Voice3: wav = assets::WAV_VOICE3; len = assets::WAV_VOICE3_LEN; break;
        case ciallo::Voice4: wav = assets::WAV_VOICE4; len = assets::WAV_VOICE4_LEN; break;
        case ciallo::Voice5: wav = assets::WAV_VOICE5; len = assets::WAV_VOICE5_LEN; break;
        case ciallo::Lemon: wav = assets::WAV_LEMON; len = assets::WAV_LEMON_LEN; break;
        case ciallo::CialloEgg: wav = assets::WAV_CIALLO; len = assets::WAV_CIALLO_LEN; break;
        default: break;
    }
    if (wav != nullptr && len > 0) {
        M5.Speaker.playWav(wav, len);
        return;
    }
#endif
    uint16_t freq = 0;
    uint32_t toneMs = 130;
    if (sound == ciallo::Start) {
        freq = 784;
    } else if (sound == ciallo::Voice1) {
        freq = 880;
    } else if (sound == ciallo::Voice2) {
        freq = 740;
    } else if (sound == ciallo::Voice3) {
        freq = 988;
    } else if (sound == ciallo::Voice4) {
        freq = 392;
        toneMs = 260;
    } else if (sound == ciallo::Voice5) {
        freq = 1046;
        toneMs = 260;
    } else if (sound == ciallo::Lemon) {
        freq = 523;
        toneMs = 220;
    } else if (sound == ciallo::CialloEgg) {
        static constexpr uint16_t eggTones[] = {660, 740, 830, 880, 990, 1174};
        freq = eggTones[eggTone % (sizeof(eggTones) / sizeof(eggTones[0]))];
        ++eggTone;
        toneMs = 160;
    } else if (sound == ciallo::Select) {
        freq = 660;
        toneMs = 70;
    }
    if (freq != 0) M5.Speaker.tone(freq, toneMs);
}

void saveGame() {
    prefs.putInt("scene", static_cast<int>(player.sceneIndex()));
    prefs.putInt("aff", player.affection());
    prefs.putBool("has", true);
    hasSave = true;
    savedScene = static_cast<int>(player.sceneIndex());
    savedAffection = player.affection();
}

void clearSave() {
    prefs.putBool("has", false);
    prefs.putInt("scene", -1);
    hasSave = false;
    savedScene = -1;
}

void loadSaveMeta() {
    hasSave = prefs.getBool("has", false);
    savedScene = prefs.getInt("scene", -1);
    savedAffection = prefs.getInt("aff", 50);
    if (savedScene < 0 || savedScene >= static_cast<int>(player.sceneCount())) {
        hasSave = false;
        savedScene = -1;
    }
}

void enterScene(uint32_t now) {
    sceneStarted = now;
    typewriterDone = false;
    lastBgm = now;
    saveGame();
    playCue(player.current().sound);
    Serial.printf("scene=%u aff=%d kind=%d\n", static_cast<unsigned>(player.sceneIndex()), player.affection(),
                  static_cast<int>(player.current().kind));
}

void checkThresholds(uint32_t now) {
    const size_t idx = player.sceneIndex();
    if (idx == ciallo::Win || idx == ciallo::Fail || idx == ciallo::Easter) return;
    if (player.affection() >= 100) {
        player.jumpTo(ciallo::Win);
        enterScene(now);
    } else if (player.affection() <= 0) {
        player.jumpTo(ciallo::Fail);
        enterScene(now);
    }
}

void confirmChoice(uint32_t now) {
    const size_t prev = player.sceneIndex();
    const size_t opt = player.selectedOption();
    if (!player.confirm()) return;
    if (player.sceneIndex() == ciallo::Easter) {
        player.restore(ciallo::Easter, 78);
        enterScene(now);
        return;
    }
    if ((prev == ciallo::Win && opt == 0) || (prev == ciallo::Fail && opt == 0)) {
        player.restore(ciallo::Step1, 50);
        enterScene(now);
        return;
    }
    enterScene(now);
    checkThresholds(now);
}

void bgmTick(uint32_t now) {
    if (mode != AppMode::Story) return;
    if (player.current().kind == Kind::Ending) return;
    if (now - lastBgm < BgmMs) return;
    lastBgm = now;
    static constexpr uint16_t melody[] = {523, 587, 659, 784, 659, 587, 523, 440};
    const uint16_t f = melody[bgmStep % (sizeof(melody) / sizeof(melody[0]))];
    ++bgmStep;
    M5.Speaker.tone(f, 160);
}

bool ensureTypewriter(uint32_t now) {
    if (typewriterDone) return true;
    const size_t total = utf8Chars(player.current().text);
    if (visibleChars(now) >= total) {
        typewriterDone = true;
        return true;
    }
    typewriterDone = true;
    return false;
}

void fireEgg(uint32_t now) {
    toastUntil = now + 1200;
    playCue(ciallo::CialloEgg);
    Serial.println("egg=ciallo");
}

void gotoTitle(uint32_t now) {
    loadSaveMeta();
    mode = AppMode::Title;
    aDown = bDown = false;
    renderTitle(now);
}

}  // namespace

void setup() {
    auto config = M5.config();
    config.internal_mic = false;
    config.internal_spk = true;
    M5.begin(config);
    M5.Display.setRotation(1);
    M5.Display.setBrightness(103);
    Serial.begin(115200);

    frame.setColorDepth(16);
    if (!frame.createSprite(M5.Display.width(), M5.Display.height())) {
        M5.Display.setFont(&fonts::efontCN_12);
        M5.Display.drawString("画面初始化失败", Margin, Margin);
        return;
    }
    M5.Speaker.begin();
    M5.Speaker.setVolume(150);
    prefs.begin("stickstory", false);
    loadSaveMeta();
    player.reset();
    mode = AppMode::Title;
    renderTitle(millis());
}

void loop() {
    M5.update();
    if (frame.width() == 0) {
        delay(20);
        return;
    }
    const uint32_t now = millis();

    if (mode == AppMode::Title) {
        if (M5.BtnA.wasPressed()) {
            player.reset();
            mode = AppMode::Story;
            enterScene(now);
        } else if (M5.BtnB.wasPressed() && hasSave) {
            player.restore(static_cast<size_t>(savedScene), savedAffection);
            mode = AppMode::Story;
            enterScene(now);
        }
        renderTitle(now);
        delay(30);
        return;
    }

    // Story mode: track holds for egg (B) and back-to-title (A).
    if (M5.BtnA.wasPressed()) {
        aDown = true;
        aDownAt = now;
        aTitleFired = false;
    }
    if (M5.BtnB.wasPressed()) {
        bDown = true;
        bDownAt = now;
        bEggFired = false;
    }
    if (aDown && !aTitleFired && M5.BtnA.isPressed() && now - aDownAt >= TitleHoldMs) {
        aTitleFired = true;
        gotoTitle(now);
        delay(10);
        return;
    }
    if (bDown && !bEggFired && M5.BtnB.isPressed() && now - bDownAt >= EggHoldMs) {
        bEggFired = true;
        fireEgg(now);
    }

    const size_t total = utf8Chars(player.current().text);
    if (!typewriterDone && visibleChars(now) >= total) typewriterDone = true;

    if (player.current().kind == Kind::Choice) {
        if (M5.BtnB.wasReleased()) {
            if (bDown && !bEggFired) {
                if (!ensureTypewriter(now)) {
                    renderStory(now);
                } else {
                    confirmChoice(now);
                }
            }
            bDown = false;
        } else if (M5.BtnA.wasPressed()) {
            if (!ensureTypewriter(now)) {
                renderStory(now);
            } else if (player.current().optionCount < 2) {
                confirmChoice(now);
            } else if (player.moveSelection()) {
                playCue(ciallo::Select);
            }
        }
    } else if (player.current().kind == Kind::Ending) {
        if (M5.BtnA.wasPressed()) {
            gotoTitle(now);
            delay(10);
            return;
        }
    } else {
        if (M5.BtnA.wasPressed()) {
            if (!ensureTypewriter(now)) {
                renderStory(now);
            } else if (player.advance()) {
                enterScene(now);
            }
        }
    }
    if (M5.BtnA.wasReleased()) aDown = false;
    if (!M5.BtnB.isPressed()) bDown = false;

    bgmTick(now);
    renderStory(now);
    delay(30);
}
