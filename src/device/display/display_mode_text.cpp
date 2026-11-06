#include "device/display/display_mode_text.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"

namespace av {

namespace {

constexpr uint32_t decimal_base = 10u;
constexpr uint32_t max_digits = 6u;

}

DisplayModeText::DisplayModeText(const char *text) : text_(text)
{
    AV_SCOPE("DisplayModeText::DisplayModeText");
}

bool DisplayModeText::empty() const
{
    AV_SCOPE("DisplayModeText::empty");
    if (text_ == nullptr) return true;
    for (const char *at = text_; *at != '\0'; at++)
        if (*at != ' ') return false;
    return true;
}

Status DisplayModeText::read(DisplayModeSet &modes) const
{
    AV_SCOPE("DisplayModeText::read");
    if (empty()) return Status::invalid_parameter;
    DisplayModeSet read;
    const char    *at = text_;
    for (;;) {
        while (*at == ' ') at++;
        uint32_t width = 0u;
        uint32_t height = 0u;
        if (!edge(at, width) || (*at != 'x' && *at != 'X')) return Status::invalid_parameter;
        at++;
        if (!edge(at, height)) return Status::invalid_parameter;
        while (*at == ' ') at++;
        const uint32_t before = read.count();
        const Status   added = read.add(width, height);
        if (added != Status::ok) return Status::invalid_parameter;
        if (read.count() == before) return Status::invalid_parameter;
        if (*at == '\0') break;
        if (*at != ',') return Status::invalid_parameter;
        at++;
    }
    static_cast<void>(read.prefer(0u));
    modes = read;
    return Status::ok;
}

bool DisplayModeText::edge(const char *&at, uint32_t &value) const
{
    AV_SCOPE("DisplayModeText::edge");
    value = 0u;
    uint32_t digits = 0u;
    for (; *at >= '0' && *at <= '9'; at++) {
        if (++digits > max_digits) return false;
        value = value * decimal_base + static_cast<uint32_t>(*at - '0');
    }
    return digits != 0u;
}

}
