#ifndef AV_SUBRESOURCE_LAYOUT_HPP
#define AV_SUBRESOURCE_LAYOUT_HPP

#include <stdint.h>

namespace av {

// Mips vary fastest, then array layers, then format planes.
class SubresourceLayout {
public:
    SubresourceLayout(uint32_t mips, uint32_t layers, uint32_t planes);

    // Zero for an empty layout or a count that cannot be named by a 32-bit index.
    uint32_t count() const;
    bool holds(uint32_t subresource) const;
    uint32_t mip_of(uint32_t subresource) const;
    uint32_t layer_of(uint32_t subresource) const;

private:
    uint32_t mips_;
    uint32_t layers_;
    uint32_t count_;
};

}  // namespace av

#endif  // AV_SUBRESOURCE_LAYOUT_HPP
