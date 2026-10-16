/* A surface format the kernel can present, both ways it is named: the DXGI
 * format a surface is made in, and the D3DDDIFORMAT the miniport's
 * descriptors scan it out as (the DxgiFormat row's). Direct3D 9 names a back
 * buffer by the second, Direct3D 10 to 12 by the first; the guest's kernel
 * surfaces, its Direct3D 9 present, Direct3D 11's open of a kernel surface and
 * the host's Direct3D 9 flips all cross through this one class. */
#ifndef AV_D3D9_PRESENT_FORMAT_HPP
#define AV_D3D9_PRESENT_FORMAT_HPP

#include <stdint.h>

namespace av {

class D3d9PresentFormat {
public:
    /* A surface of the DXGI format `dxgi`. */
    explicit D3d9PresentFormat(uint32_t dxgi);

    /* The DXGI format a surface of the D3DDDIFORMAT `ddi` is presented in:
     * the typed, linear member of the family the row names it for. False,
     * and nothing changed, for one the kernel cannot present. */
    [[nodiscard]] bool take_ddi(uint32_t ddi);
    uint32_t           dxgi() const;
    /* The D3DDDIFORMAT it is scanned out as: a format with no exact
     * counterpart as A8B8G8R8. */
    uint32_t           ddi() const;
    /* The DXGI format Direct3D 10 to 12 opens a kernel surface of it as: its
     * own, except B8G8R8X8, opened as B8G8R8A8 - DWM samples every window
     * surface GDI draws as BGRA, and a view in B8G8R8A8 must be of its own
     * family. */
    uint32_t           viewed() const;

private:
    uint32_t dxgi_;
};

}  // namespace av

#endif  // AV_D3D9_PRESENT_FORMAT_HPP
