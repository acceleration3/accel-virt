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
    void woken() const;

private:
    uint64_t lock_;
};

}

#endif
