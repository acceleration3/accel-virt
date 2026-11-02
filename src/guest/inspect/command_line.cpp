#include "guest/inspect/command_line.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint16_t first_printable = 0x20u;
constexpr uint16_t last_printable = 0x7Eu;

}

CommandLine::CommandLine() : text_{}, starts_{}, count_(0u)
{
    AV_SCOPE("CommandLine::CommandLine");
}

Status CommandLine::parse(const uint16_t *line)
{
    AV_SCOPE("CommandLine::parse");
    clear();
    if (line == nullptr) return Status::malformed;
    uint32_t words = 0u;
    uint32_t used = 0u;
    uint32_t characters = 0u;
    bool     quoted = false;
    bool     inside = false;
    uint32_t firsts[max_arguments + 1u] = {};
    for (const uint16_t *unit = line; *unit != 0u; unit++) {
        if (!quoted && (*unit == u' ' || *unit == u'\t')) {
            if (inside) text_[used++] = '\0';
            inside = false;
            continue;
        }
        const bool quote = *unit == u'"';
        const bool printable = *unit >= first_printable && *unit <= last_printable;
        const bool starts = !inside;
        if (!printable || (starts && words == max_arguments + 1u) || (!quote && characters == max_bytes)) {
            clear();
            return Status::malformed;
        }
        if (starts) firsts[words++] = used;
        inside = true;
        if (quote) {
            quoted = !quoted;
            continue;
        }
        text_[used++] = static_cast<char>(*unit);
        characters++;
    }
    if (quoted) {
        clear();
        return Status::malformed;
    }
    text_[used] = '\0';
    for (uint32_t word = 1u; word < words; word++) starts_[word - 1u] = firsts[word];
    count_ = words == 0u ? 0u : words - 1u;
    return Status::ok;
}

uint32_t CommandLine::count() const
{
    AV_SCOPE("CommandLine::count");
    return count_;
}

const char *CommandLine::argument(uint32_t index) const
{
    AV_SCOPE("CommandLine::argument");
    return index < count_ ? text_ + starts_[index] : "";
}

bool CommandLine::number(uint32_t index, uint32_t &value) const
{
    AV_SCOPE("CommandLine::number");
    const char *digits = argument(index);
    uint64_t    parsed = 0u;
    if (*digits == '\0') return false;
    for (; *digits != '\0'; digits++) {
        if (*digits < '0' || *digits > '9') return false;
        parsed = parsed * 10u + static_cast<uint64_t>(*digits - '0');
        if (parsed > 0xFFFFFFFFull) return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool CommandLine::invariants_hold() const
{
    AV_SCOPE("CommandLine::invariants_hold");
    if (count_ > max_arguments) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        uint32_t at = starts_[index];
        while (at < sizeof text_ && text_[at] != '\0') at++;
        if (at >= sizeof text_) return false;
    }
    return true;
}

void CommandLine::clear()
{
    AV_SCOPE("CommandLine::clear");
    for (char &byte : text_) byte = '\0';
    count_ = 0u;
}

}
