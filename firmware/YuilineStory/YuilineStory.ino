#include <M5Unified.h>
#include <esp_random.h>

#include "Continuation.h"

#include "Timeline.h"
#include "YuilineData.h"

namespace {

using yuiline::Blob;
using yuiline::Portrait;
using yuiline::Sound;

enum class Screen { Title, Story, AiWaiting, AiStory, AiChoice, AiError };
Screen screen = Screen::Title;
M5Canvas frame(&M5.Display);
bool displayReady = false;
bool speakerReady = false;
size_t line = 0;
yuiline::Continuation continuation;
uint32_t aiSession = 0;
uint16_t aiTurn = 0;
uint32_t aiAttempt = 0;
uint32_t waitStartedAt = 0;
uint32_t requestSentAt = 0;
bool acknowledged = false;
bool warmAcknowledged = false;
bool warmSent = false;
uint32_t warmSentAt = 0;
bool waitShown = false;
uint32_t waitDisplayAt = 0;
int requestedChoice = -1;
size_t aiLine = 0;
size_t aiChoice = 0;

constexpr int Width = 240;
constexpr int Height = 135;
constexpr int TextTop = 77;
constexpr int Margin = 8;
constexpr uint16_t Panel = 0x10C5;
constexpr uint16_t Accent = 0x653E;
constexpr uint8_t MusicChannel = 0;
constexpr uint8_t CrowdChannel = 1;
constexpr uint8_t RiceChannel = 2;
constexpr uint8_t HeelsChannel = 3;
constexpr uint8_t WaterChannel = 4;
constexpr uint8_t MusicVolume = 77;
constexpr uint8_t WaterVolume = 128;
constexpr uint32_t WaterFadeMs = 1500;
constexpr uint32_t PortraitFadeMs = 1200;
constexpr unsigned FadeSteps = 3;

bool crossfading = false;
uint32_t audioStarted = 0;
uint32_t portraitStarted = 0;
unsigned opacityFrom = 0;
unsigned opacityTo = 0;
Portrait portrait = Portrait::Hidden;
const Blob* previousFrame = nullptr;

void drawImage(const Blob& image, int x = 0, int y = 0) {
    frame.drawJpg(image.data, image.size, x, y);
}

void play(const Blob& audio, uint8_t channel, uint8_t volume, uint32_t repeat = 1) {
    if (!speakerReady) return;
    M5.Speaker.setChannelVolume(channel, volume);
    M5.Speaker.playRaw(audio.data, audio.size, yuiline::SampleRate, false, repeat, channel, true);
}

void stopAudio() {
    crossfading = false;
    if (speakerReady) M5.Speaker.stop();
}

void startCue(Sound sound, uint32_t now) {
    switch (sound) {
        case Sound::None: break;
        case Sound::Crowd: play(yuiline::Crowd, CrowdChannel, 77); break;
        case Sound::Rice: play(yuiline::Rice, RiceChannel, 77); break;
        case Sound::Heels: play(yuiline::Heels, HeelsChannel, 77); break;
        case Sound::Water:
            play(yuiline::Water, WaterChannel, 0);
            audioStarted = now;
            crossfading = speakerReady;
            break;
    }
}

void updateAudio(uint32_t now) {
    if (!crossfading) return;
    uint32_t elapsed = now - audioStarted;
    if (elapsed >= WaterFadeMs) elapsed = WaterFadeMs;

    M5.Speaker.setChannelVolume(MusicChannel, MusicVolume * (WaterFadeMs - elapsed) / WaterFadeMs);
    M5.Speaker.setChannelVolume(WaterChannel, WaterVolume * elapsed / WaterFadeMs);
    if (elapsed == WaterFadeMs) {
        M5.Speaker.stop(MusicChannel);
        crossfading = false;
    }
}

unsigned portraitOpacity(uint32_t now) {
    const auto elapsed = now - portraitStarted;
    if (elapsed >= PortraitFadeMs) return opacityTo;
    const unsigned value = opacityFrom * (PortraitFadeMs - elapsed) + opacityTo * elapsed;
    return (value + PortraitFadeMs / 2) / PortraitFadeMs;
}

void setPortrait(Portrait next, uint32_t now) {
    if (next == Portrait::Hidden) {
        if (opacityTo == 0) return;
        opacityFrom = portraitOpacity(now);
        opacityTo = 0;
    } else {
        if (next == portrait && opacityTo == FadeSteps) return;
        portrait = next;
        opacityFrom = 0;
        opacityTo = FadeSteps;
    }
    portraitStarted = now;
}

const Blob& backdrop(uint32_t now) {
    if (yuiline::beatAt(line).night) return yuiline::Night;
    const unsigned opacity = portraitOpacity(now);
    if (opacity == 0 || portrait == Portrait::Hidden) return yuiline::Day;
    static const Blob* const normal[] = {&yuiline::Normal1, &yuiline::Normal2, &yuiline::Normal3};
    static const Blob* const sad[] = {&yuiline::Sad1, &yuiline::Sad2, &yuiline::Sad3};
    return *(portrait == Portrait::Normal ? normal[opacity - 1] : sad[opacity - 1]);
}

void drawWrapped(const char* text, int x, int y, int maxWidth) {
    frame.setTextDatum(top_left);
    frame.setFont(&fonts::efontCN_12);
    String row;

    const int rowHeight = frame.fontHeight();
    while (*text != '\0') {
        const char* start = text++;
        while ((static_cast<uint8_t>(*text) & 0xC0) == 0x80) ++text;
        if (*start != '\n') {
            String candidate = row;
            candidate.concat(start, static_cast<unsigned int>(text - start));
            if (row.isEmpty() || frame.textWidth(candidate) <= maxWidth) {
                row = candidate;
                continue;
            }
        }

        frame.drawString(row, x, y);
        y += rowHeight;
        row = "";
        if (*start != '\n') row.concat(start, static_cast<unsigned int>(text - start));
    }
    if (!row.isEmpty()) frame.drawString(row, x, y);
}

void renderStory(uint32_t now) {
    const Blob& image = backdrop(now);
    drawImage(image);
    frame.fillRect(0, TextTop, Width, Height - TextTop, Panel);
    frame.drawFastHLine(0, TextTop, Width, Accent);
    frame.setTextColor(TFT_WHITE);
    drawWrapped(yuiline::Lines[line], Margin, TextTop + 5, Width - 2 * Margin);

    constexpr int Footer = Height - 14;
    drawImage(yuiline::Back, Margin, Footer);
    frame.setTextColor(Accent);
    frame.drawString("B 返回", Margin + 21, Footer);
    frame.setTextDatum(top_right);
    char position[24];
    snprintf(position, sizeof(position), "%u/%u  A 继续",
             static_cast<unsigned>(line + 1), static_cast<unsigned>(yuiline::LineCount));
    frame.drawString(position, Width - Margin, Footer);
    frame.pushSprite(0, 0);
    previousFrame = &image;
}

void renderMenu() {
    drawImage(yuiline::Day);
    frame.fillRoundRect(17, 38, Width - 34, 67, 6, Panel);
    frame.setTextDatum(top_center);
    frame.setTextColor(TFT_WHITE);
    frame.setFont(&fonts::efontCN_16);
    frame.drawString("galgame by yuiline", Width / 2, 46);
    frame.setFont(&fonts::efontCN_12);
    frame.setTextColor(Accent);
    frame.drawString("A 开始游戏", Width / 2, 72);
    frame.drawString("B 直接续写第一章", Width / 2, 86);
    frame.pushSprite(0, 0);
}

void enterLine(uint32_t now) {
    const auto beat = yuiline::beatAt(line);
    setPortrait(beat.portrait, now);
    startCue(beat.sound, now);
    renderStory(now);
}

uint32_t randomId() {
    const uint32_t value = esp_random();
    return value == 0 ? 1 : value;
}

void resetSession() {
    aiSession = randomId();
    aiTurn = 0;
    warmSent = false;
    warmAcknowledged = false;
}

void prepareContinuation(uint32_t now) {
    if (line + 20 < yuiline::LineCount || warmAcknowledged) return;
    if (warmSent && now - warmSentAt < 2500) return;
    Serial.printf("VN WARM %lu\n", static_cast<unsigned long>(aiSession));
    warmSentAt = now;
    warmSent = true;
}

void startStory(uint32_t now) {
    resetSession();
    screen = Screen::Story;
    line = 0;
    portrait = Portrait::Hidden;
    opacityFrom = opacityTo = 0;
    portraitStarted = now;
    play(yuiline::Jazz, MusicChannel, MusicVolume, 0);
    enterLine(now);
}

void showTitle() {
    if (aiSession != 0) {
        Serial.printf("VN STOP %lu\n", static_cast<unsigned long>(aiSession));
        aiSession = 0;
    }
    stopAudio();
    screen = Screen::Title;
    renderMenu();
}

void renderAi() {
    drawImage(yuiline::Night);
    frame.setFont(&fonts::efontCN_12);
    frame.setTextDatum(top_left);
    frame.setTextColor(TFT_WHITE);

    if (screen == Screen::AiStory) {
        frame.fillRect(0, TextTop, Width, Height - TextTop, Panel);
        frame.drawFastHLine(0, TextTop, Width, Accent);
        drawWrapped(continuation.lines[aiLine], Margin, TextTop + 5, Width - 2 * Margin);
        frame.setTextColor(Accent);
        frame.drawString("B 返回", Margin, Height - 14);
        frame.setTextDatum(top_right);
        frame.drawString(aiLine + 1 == continuation.lineCount ? "A 查看选项" : "A 继续",
                         Width - Margin, Height - 14);
    } else if (screen == Screen::AiChoice) {
        frame.fillRoundRect(8, 16, Width - 16, 108, 6, Panel);
        frame.drawString("请选择接下来的行动", 16, 23);
        for (size_t index = 0; index < yuiline::Continuation::OptionCount; ++index) {
            const int y = 43 + static_cast<int>(index) * 29;
            const bool selected = index == aiChoice;
            frame.fillRoundRect(15, y, Width - 30, 26, 4, selected ? Accent : 0x21C9);
            frame.setTextColor(selected ? Panel : TFT_WHITE);
            drawWrapped(continuation.options[index], 21, y + 1, Width - 42);
        }
        frame.setTextColor(Accent);
        frame.drawString("A 切换  B 确认  长按 B 返回", 16, 105);
    } else {
        frame.fillRoundRect(8, 39, Width - 16, 62, 6, Panel);
        frame.setTextDatum(top_center);
        frame.drawString(screen == Screen::AiWaiting ? "正在续写剧情…" : continuation.error, Width / 2, 49);
        frame.setTextColor(Accent);
        frame.drawString(screen == Screen::AiWaiting ? "B 返回标题" : "A 重试   B 返回标题", Width / 2, 75);
    }
    frame.pushSprite(0, 0);
}



void sendRequest(uint32_t now) {
    Serial.printf("VN REQ %lu %u %d %lu\n", static_cast<unsigned long>(aiSession),
                  static_cast<unsigned>(aiTurn), requestedChoice, static_cast<unsigned long>(aiAttempt));
    requestSentAt = now;
}

void requestScene(int choice, uint32_t now) {
    requestedChoice = choice;
    aiAttempt = randomId();
    continuation.reset(aiSession, aiTurn, aiAttempt);
    screen = Screen::AiWaiting;
    acknowledged = false;
    waitStartedAt = now;
    waitDisplayAt = now;
    waitShown = false;
    sendRequest(now);
}

void startContinuation(uint32_t now) {
    stopAudio();
    if (screen == Screen::Title || aiSession == 0 || aiTurn == UINT16_MAX) resetSession();
    requestScene(-1, now);
}

void showAiError(const char* reason) {
    if (reason != continuation.error) {
        snprintf(continuation.error, sizeof(continuation.error), "%s", reason);
    }
    screen = Screen::AiError;
    renderAi();
}

void handleSerialLine(const char* message, uint32_t now) {
    if (strcmp(message, "VN PING") == 0) {
        Serial.println("VN READY 1");
        return;
    }
    unsigned long warmed = 0;
    int end = 0;
    if (sscanf(message, "VN WARMED %lu%n", &warmed, &end) == 1 && message[end] == '\0') {
        if (warmed == aiSession) warmAcknowledged = true;
        return;
    }
    if (screen != Screen::AiWaiting) return;
    switch (continuation.receive(message)) {
        case yuiline::Reply::Acknowledged:
            if (!acknowledged) waitStartedAt = now;
            acknowledged = true;
            break;
        case yuiline::Reply::Complete:
            aiLine = 0;
            screen = Screen::AiStory;
            renderAi();
            break;
        case yuiline::Reply::Failed:
            showAiError(continuation.error);
            break;
        case yuiline::Reply::Ignore:
            break;
    }
}

void pollSerial(uint32_t now) {
    static char record[224] = {};
    static size_t used = 0;
    static bool overflow = false;
    for (int count = 0; count < 256 && Serial.available(); ++count) {
        const int byte = Serial.read();
        if (byte == '\n') {
            if (!overflow) {
                record[used] = '\0';
                handleSerialLine(record, now);
            }
            used = 0;
            overflow = false;
        } else if (byte != '\r' && byte >= 0) {
            if (used + 1 < sizeof(record)) record[used++] = static_cast<char>(byte);
            else overflow = true;
        }
    }
}

void updateWait(uint32_t now) {
    if (!waitShown && now - waitDisplayAt >= 200) {
        waitShown = true;
        renderAi();
    }
    const uint32_t elapsed = now - waitStartedAt;
    if (!acknowledged && elapsed >= 7000) {
        showAiError("电脑续写服务未连接");
    } else if (acknowledged && elapsed >= 55000) {
        showAiError("续写超时");
    } else if (now - requestSentAt >= (acknowledged ? 3000 : 1250)) {
        sendRequest(now);
    }
}

}  // namespace

void setup() {
    auto config = M5.config();
    config.internal_mic = false;
    config.internal_spk = true;
    M5.begin(config);
    M5.Display.setRotation(1);
    Serial.setRxBufferSize(4096);
    Serial.begin(921600);

    frame.setColorDepth(16);
    displayReady = frame.createSprite(Width, Height) != nullptr;
    if (!displayReady) {
        M5.Display.setFont(&fonts::efontCN_12);
        M5.Display.drawString("画面初始化失败", Margin, Margin);
        return;
    }

    speakerReady = M5.Speaker.begin();
    if (speakerReady) M5.Speaker.setVolume(160);
    renderMenu();
}

void loop() {
    M5.update();
    if (!displayReady) {
        delay(20);
        return;
    }

    const uint32_t now = millis();
    const bool a = M5.BtnA.wasPressed();
    const bool b = M5.BtnB.wasPressed();
    switch (screen) {
        case Screen::Title:
            if (a) startStory(now);
            else if (b) startContinuation(now);
            break;
        case Screen::Story:
            if (b) {
                showTitle();
            } else if (a) {
                ++line;
                if (line >= yuiline::LineCount) startContinuation(now);
                else enterLine(now);
            } else if (&backdrop(now) != previousFrame) {
                renderStory(now);
            }
            updateAudio(now);
            if (screen == Screen::Story) prepareContinuation(now);
            break;
        case Screen::AiWaiting:
            if (b) showTitle();
            else updateWait(now);
            break;
        case Screen::AiStory:
            if (b) {
                showTitle();
            } else if (a) {
                if (++aiLine >= continuation.lineCount) {
                    aiChoice = 0;
                    screen = Screen::AiChoice;
                }
                renderAi();
            }
            break;
        case Screen::AiChoice:
            if (M5.BtnB.pressedFor(700) || M5.BtnB.wasReleaseFor(700)) {
                showTitle();
            } else if (M5.BtnB.wasReleased()) {
                if (aiTurn == UINT16_MAX) {
                    startContinuation(now);
                } else {
                    ++aiTurn;
                    requestScene(static_cast<int>(aiChoice), now);
                }
            } else if (a) {
                aiChoice = (aiChoice + 1) % yuiline::Continuation::OptionCount;
                renderAi();
            }
            break;
        case Screen::AiError:
            if (b) showTitle();
            else if (a) requestScene(requestedChoice, now);
            break;
    }
    pollSerial(now);
    delay(10);
}
