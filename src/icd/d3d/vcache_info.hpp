#ifndef AV_ICD_D3D_VCACHE_INFO_HPP
#define AV_ICD_D3D_VCACHE_INFO_HPP

#include <stdint.h>

namespace av::d3d {

class VcacheInfo {
public:
    static constexpr uint32_t pattern_cach = 0x48434143u;
    static constexpr uint32_t bytes = 16u;

    VcacheInfo();

    [[nodiscard]] bool write(void *to, uint64_t room) const;

private:
    uint32_t pattern_;
    uint32_t optimization_;
    uint32_t cache_size_;
    uint32_t magic_;
};

}

#endif
