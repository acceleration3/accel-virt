/* The order several recording locks were taken and let go in, as one string: a
 * lock's letter when it is acquired, the same letter in lower case when it is
 * released. "SHhs" is H taken inside S; "hH" is a release of H before its
 * acquire. What a test asserts lock order with. */
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

    /* Past the capacity the trace is cut, and says so with a final '~'. */
    void        append(char event);
    const char *text() const;
    void        clear();

private:
    char          text_[capacity + 1u];
    std::uint32_t length_;
};

}  // namespace av

#endif  // AV_LOCK_TRACE_HPP
