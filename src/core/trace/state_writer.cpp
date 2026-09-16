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

void StateWriter::text(const char *name, const char *value)
{
    key(name);
    out_.append(value != nullptr ? value : "");
}

void StateWriter::flag(const char *name, bool value) { text(name, value ? "true" : "false"); }

const char *StateWriter::dump() const { return out_.text(); }
size_t      StateWriter::length() const { return out_.length(); }

/* TextBuffer keeps one byte for the terminator and cuts silently, so a full
 * buffer is the only sign of a cut; a dump that exactly fills one counts. */
bool StateWriter::truncated() const { return capacity_ != 0u && out_.length() + 1u >= capacity_; }

/* Every line still open was opened, and nothing is written past the
 * terminator's byte. */
bool StateWriter::invariants_hold() const
{
    if (depth_ > opened_) return false;
    return capacity_ == 0u ? out_.length() == 0u : out_.length() < capacity_;
}

void StateWriter::key(const char *name)
{
    out_.append(" ");
    out_.append(name != nullptr ? name : "?");
    out_.append("=");
}

}  // namespace av
