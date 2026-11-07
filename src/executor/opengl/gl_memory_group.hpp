#ifndef AV_GL_MEMORY_GROUP_HPP
#define AV_GL_MEMORY_GROUP_HPP

#include "device/shim/posix_lock.hpp"

#include <stdint.h>

namespace av {

class GlMemoryObjects;

class GlMemoryGroup {
public:
    static constexpr uint32_t most_members = 64u;

    GlMemoryGroup();
    GlMemoryGroup(const GlMemoryGroup &) = delete;
    GlMemoryGroup &operator=(const GlMemoryGroup &) = delete;

    void lock();
    void unlock();

    [[nodiscard]] bool join(GlMemoryObjects &member);
    void               part(GlMemoryObjects &member);

    uint32_t         members() const;
    GlMemoryObjects *at(uint32_t index) const;

private:
    PosixLock        lock_;
    GlMemoryObjects *members_[most_members];
    uint32_t         count_;
};

}

#endif
