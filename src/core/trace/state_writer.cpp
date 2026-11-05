#include "core/trace/state_writer.hpp"

namespace av {

StateWriter::StateWriter(char *destination, size_t capacity)
    : out_(destination, capacity), capacity_(destination != nullptr ? capacity : 0u), kinds_{}, indices_{},
      depth_(0u), opened_(0u)
{}

void StateWriter::open(const char *kind, uint64_t index)
{
    if (depth_ < max_depth) {
        kinds_[depth_] = kind != nullptr ? kind : "?";
        indices_[depth_] = index;
    }
    depth_++;
    if (opened_++ != 0u) out_.append("\n");
    const uint32_t kept = depth_ < max_depth ? depth_ : max_depth;
    for (uint32_t level = 0u; level < kept; level++) {
        if (level != 0u) out_.append("/");
        out_.append(kinds_[level]);
        out_.append("/");
        out_.append_decimal(indices_[level]);
    }
}

void StateWriter::close()
{
    if (depth_ != 0u) depth_--;
}

void StateWriter::number(const char *name, uint64_t value)
{
    key(name);
    out_.append_decimal(value);
}

void StateWriter::hex(const char *name, uint64_t value)
{
    key(name);
    out_.append("0x");
    out_.append_hex(value, hex_digits);
}

void StateWriter::address(const char *name, const void *object)
{
    hex(name, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object)));
}

void StateWriter::text(const char *name, const char *value)
{
    key(name);
    append_escaped(value != nullptr ? value : "");
}

void StateWriter::flag(const char *name, bool value) { text(name, value ? "true" : "false"); }

const char *StateWriter::dump() const { return out_.text(); }
size_t      StateWriter::length() const { return out_.length(); }

bool StateWriter::truncated() const { return capacity_ != 0u && out_.length() + 1u >= capacity_; }

bool StateWriter::invariants_hold() const
{
    if (depth_ > opened_) return false;
    return capacity_ == 0u ? out_.length() == 0u : out_.length() < capacity_;
}

void StateWriter::append_escaped(const char *value)
{
    for (const char *at = value; *at != '\0'; at++) {
        const auto byte = static_cast<unsigned char>(*at);
        char       piece[2] = {*at, '\0'};
        if (byte == '\n' || byte == '\r') {
            out_.append("\\");
            piece[0] = byte == '\n' ? 'n' : 'r';
        } else if (byte == '=' || byte == '\\') {
            out_.append("\\");
        } else if (byte < 0x20u || byte == 0x7Fu) {
            out_.append("\\x");
            out_.append_hex(byte, 2u);
            continue;
        }
        out_.append(piece);
    }
}

void StateWriter::key(const char *name)
{
    out_.append(" ");
    out_.append(name != nullptr ? name : "?");
    out_.append("=");
}

}
