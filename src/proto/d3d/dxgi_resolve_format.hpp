/* The format a multisampled texture of one DXGI format is resolved in.
 * ResolveSubresource takes a typed format, and Direct3D 9 on 12 makes its
 * back buffers in a typeless family, so it can draw them in sRGB or not. */
#ifndef AV_DXGI_RESOLVE_FORMAT_HPP
#define AV_DXGI_RESOLVE_FORMAT_HPP

#include <stdint.h>

namespace av {

class DxgiResolveFormat {
public:
    explicit DxgiResolveFormat(uint32_t format);

    /* The format itself when it is typed; the UNORM member of a four-byte
     * colour family for its typeless head; 0 for any other typeless format
     * and for UNKNOWN, none of which a presented texture is. */
    uint32_t typed() const;

private:
    uint32_t format_;
};

}  // namespace av

#endif  // AV_DXGI_RESOLVE_FORMAT_HPP
