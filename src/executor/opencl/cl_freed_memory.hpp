#ifndef AV_CL_FREED_MEMORY_HPP
#define AV_CL_FREED_MEMORY_HPP

#include <stdint.h>

namespace av {

class ClFreedMemory {
public:
    ClFreedMemory();
    ClFreedMemory(const ClFreedMemory &) = delete;
    ClFreedMemory &operator=(const ClFreedMemory &) = delete;

    static constexpr uint64_t saturated = ~uint64_t{0};

    void     add(uint64_t reach);
    uint64_t total() const;

private:
    uint64_t total_;
};

}

#endif
