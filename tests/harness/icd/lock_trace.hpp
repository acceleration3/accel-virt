#ifndef AV_LOCK_TRACE_HPP
#define AV_LOCK_TRACE_HPP

#include <cstdint>

namespace av {

class LockTrace {
public:
    static constexpr std::uint32_t capacity = 256u;

    LockTrace();
    LockTrace(const LockTrace &) = delete;
    LockTrace &operator=(const LockTrace &) = delete;

    void        append(char event);
    const char *text() const;
    void        clear();

private:
    char          text_[capacity + 1u];
    std::uint32_t length_;
};

}

#endif
