/* One run of a guest blob's pages as the renderer reaches them: an offset into
 * one of the guest RAM regions QEMU sent it, written as
 * `(region << region_shift) | offset` so an extent is two words whatever the
 * region (docs/renderer.md). Guest physical addresses mean nothing in the
 * renderer, which maps each region's memfd on its own. */
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
    /* The largest region an offset can address: 1 TiB. */
    static constexpr uint64_t region_span = uint64_t{1} << region_shift;
    /* What is left of the 64-bit address above the offset. */
    static constexpr uint32_t region_count = 1u << (64u - region_shift);

    RendererExtent();
    RendererExtent(uint32_t region, uint64_t offset, uint64_t length);

    uint32_t region() const;
    uint64_t offset() const;
    uint64_t length() const;
    /* A region the address can name, and a non-empty run that ends inside it. */
    bool     fits() const;

    /* The address, then the length. */
    void write(WireWriter &writer) const;
    /* Reads sixteen bytes, whatever they hold; fits() judges them. */
    void read(WireReader &reader);

private:
    uint32_t region_;
    uint64_t offset_;
    uint64_t length_;
};

}  // namespace av

#endif  // AV_RENDERER_EXTENT_HPP
