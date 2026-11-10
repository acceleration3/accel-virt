#ifndef AV_RESOURCE_MISC_HPP
#define AV_RESOURCE_MISC_HPP

#include <stdint.h>

namespace av {

class ResourceMisc {
public:
    static constexpr uint32_t shared_bit = 0x2u;
    static constexpr uint32_t tile_pool_bit = 0x20000u;
    static constexpr uint32_t tiled_bit = 0x40000u;
    static constexpr uint32_t rows_bit = 0x80000000u;
    static constexpr uint32_t presented_bit = 0x40000000u;

    explicit ResourceMisc(uint32_t flags);

    bool     shared() const;
    bool     rows() const;
    bool     presented() const;

private:
    uint32_t flags_;
};

}

#endif
