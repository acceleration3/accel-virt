#ifndef AV_CLICD_D3D11_RESOURCE_HPP
#define AV_CLICD_D3D11_RESOURCE_HPP

#include <stdint.h>

namespace av {
class MipExtent;
}

namespace av::clicd {

class D3d11Resource {
public:
    enum class Kind : uint32_t { none, buffer, texture2d, texture3d };

    D3d11Resource();
    D3d11Resource(Kind kind, uint32_t format, uint64_t bytes, uint32_t width, uint32_t height, uint32_t depth,
                  uint32_t mips, uint32_t layers, uint32_t samples);

    Kind     kind() const;
    uint32_t format() const;
    uint64_t bytes() const;
    bool     multisampled() const;
    bool     holds(uint32_t subresource) const;
    uint32_t width_of(uint32_t subresource) const;
    uint32_t height_of(uint32_t subresource) const;
    uint32_t depth_of(uint32_t subresource) const;

private:
    MipExtent extent_of(uint32_t subresource) const;

    Kind     kind_;
    uint32_t format_;
    uint64_t bytes_;
    uint32_t width_;
    uint32_t height_;
    uint32_t depth_;
    uint32_t mips_;
    uint32_t layers_;
    uint32_t samples_;
};

}

#endif
