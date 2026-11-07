#include "proto/context/mem_info_text.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr char key[] = "MemTotal:";
constexpr size_t key_length = sizeof key - 1u;

}

MemInfoText::MemInfoText(const char *text, size_t length) : text_(text), length_(text == nullptr ? 0u : length)
{
    AV_SCOPE("MemInfoText::MemInfoText");
}

uint64_t MemInfoText::total_bytes() const
{
    AV_SCOPE("MemInfoText::total_bytes");
    size_t at = 0u;
    while (at < length_) {
        const bool found = length_ - at >= key_length && __builtin_memcmp(text_ + at, key, key_length) == 0;
        if (found) {
            at += key_length;
            while (at < length_ && (text_[at] == ' ' || text_[at] == '\t')) at++;
            uint64_t kib = 0u;
            bool     digits = false;
            while (at < length_ && text_[at] >= '0' && text_[at] <= '9') {
                const uint64_t digit = static_cast<uint64_t>(text_[at] - '0');
                if (kib > (UINT64_MAX / 1024u - digit) / 10u) return 0u;
                kib = kib * 10u + digit;
                digits = true;
                at++;
            }
            return digits ? kib * 1024u : 0u;
        }
        while (at < length_ && text_[at] != '\n') at++;
        at++;
    }
    return 0u;
}

}
