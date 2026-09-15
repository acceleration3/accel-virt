#include "harness/icd/lock_trace.hpp"

namespace av {

LockTrace::LockTrace() : text_{}, length_(0u) {}

void LockTrace::append(char event)
{
    if (length_ < capacity) {
        text_[length_++] = event;
        return;
    }
    text_[capacity - 1u] = '~';
}

const char *LockTrace::text() const { return text_; }

void LockTrace::clear()
{
    for (char &slot : text_) slot = '\0';
    length_ = 0u;
}

}  // namespace av
