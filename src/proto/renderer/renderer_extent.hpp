#ifndef AV_RENDERER_EXTENT_HPP
#define AV_RENDERER_EXTENT_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class RendererExtent {
public:
    static constexpr size_t   encoded_size = 16u;
    static constexpr uint32_t region_shift = 40u;
    static constexpr uint64_t region_span = uint64_t{1} << region_shift;
    static constexpr uint32_t region_count = 1u << (64u - region_shift);

    RendererExtent();
    RendererExtent(uint32_t region, uint64_t offset, uint64_t length);

    uint32_t region() const;
    uint64_t offset() const;
    uint64_t length() const;
    bool     fits() const;

    void write(WireWriter &writer) const;
    void read(WireReader &reader);

private:
    uint32_t region_;
    uint64_t offset_;
    uint64_t length_;
};

}

#endif
