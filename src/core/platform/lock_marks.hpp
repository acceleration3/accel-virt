/* What one lock tells the bound trace table (docs/trace.md): that the calling
 * thread waits on it, got it, gave it back - so a hang shows which thread
 * waits on which lock and who holds it. Nothing while no table is bound. */
#ifndef AV_LOCK_MARKS_HPP
#define AV_LOCK_MARKS_HPP

#include <stdint.h>

namespace av {

class LockMarks {
public:
    explicit LockMarks(const void *lock);

    void waiting() const;
    void acquired() const;
    void released() const;

private:
    uint64_t lock_;
};

}  // namespace av

#endif  // AV_LOCK_MARKS_HPP
