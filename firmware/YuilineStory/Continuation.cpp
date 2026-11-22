#include "Continuation.h"

#include <stdio.h>
#include <string.h>

namespace yuiline {
namespace {

bool copyText(char* dest, size_t capacity, const char* text) {
    const size_t size = strlen(text);
    if (size == 0 || size >= capacity) return false;
    memcpy(dest, text, size + 1);
    return true;
}

}  // namespace

void Continuation::reset(uint32_t session, uint16_t turn, uint32_t attempt) {
    session_ = session;
    turn_ = turn;
    attempt_ = attempt;
    lineCount = 0;
    optionsReceived_ = 0;
    receiving_ = false;
    error[0] = '\0';
}

Reply Continuation::fail(const char* reason) {
    receiving_ = false;
    if (!copyText(error, sizeof(error), reason)) {
        strcpy(error, "续写数据异常");
    }
    return Reply::Failed;
}

Reply Continuation::receive(const char* message) {
    unsigned long session = 0, turn = 0, attempt = 0;
    char kind[8] = {};
    int offset = 0;
    if (sscanf(message, "VN %lu %lu %lu %7s%n", &session, &turn, &attempt, kind, &offset) != 4 ||
        session != session_ || turn != turn_ || attempt != attempt_) {
        return Reply::Ignore;
    }

    const char* text = message + offset;
    if (*text == ' ') ++text;
    if (strcmp(kind, "ACK") == 0 && *text == '\0') return Reply::Acknowledged;
    if (strcmp(kind, "ERR") == 0) return fail(text);
    if (strcmp(kind, "BEGIN") == 0) {
        unsigned count = 0;
        char extra = 0;
        if (sscanf(text, "%u%c", &count, &extra) != 1 || count < 2 || count > MaxLines) {
            return fail("续写页数异常");
        }
        expectedLines_ = count;
        lineCount = 0;
        optionsReceived_ = 0;
        receiving_ = true;
        return Reply::Ignore;
    }
    if (!receiving_) return Reply::Ignore;

    if (strcmp(kind, "LINE") == 0 && lineCount < expectedLines_ && optionsReceived_ == 0) {
        if (copyText(lines[lineCount], sizeof(lines[0]), text)) {
            ++lineCount;
            return Reply::Ignore;
        }
    } else if (strcmp(kind, "OPTION") == 0 && optionsReceived_ < OptionCount && lineCount == expectedLines_) {
        if (copyText(options[optionsReceived_], sizeof(options[0]), text)) {
            ++optionsReceived_;
            return Reply::Ignore;
        }
    } else if (strcmp(kind, "END") == 0 && *text == '\0' &&
               lineCount == expectedLines_ && optionsReceived_ == OptionCount) {
        receiving_ = false;
        return Reply::Complete;
    }
    return fail("续写数据不完整");
}

}  // namespace yuiline
