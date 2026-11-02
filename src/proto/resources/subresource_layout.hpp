#ifndef AV_SUBRESOURCE_LAYOUT_HPP
#define AV_SUBRESOURCE_LAYOUT_HPP

#include <stdint.h>

namespace av {
class SubresourceLayout {
public:
    SubresourceLayout(uint32_t mips, uint32_t layers, uint32_t planes);

    uint32_t count() const;
    bool holds(uint32_t subresource) const;
    uint32_t mip_of(uint32_t subresource) const;
    uint32_t layer_of(uint32_t subresource) const;

private:
    uint32_t mips_;
    uint32_t layers_;
    uint32_t count_;
};

}

#endif
