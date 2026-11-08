#ifndef AV_MEMORY_REUSE_HPP
#define AV_MEMORY_REUSE_HPP

#include <stdint.h>

namespace av {

class MemoryReuse {
public:
    MemoryReuse();

    void freed(uint32_t type, uint32_t domain);
    bool crosses_domains(uint32_t type, uint32_t domain) const;

private:
    static constexpr uint32_t type_count = 32u;
    static constexpr uint32_t never_freed = 0xFFFFFFFFu;
    static constexpr uint32_t freed_by_several = 0xFFFFFFFEu;

    uint32_t last_[type_count];
};

}

#endif
