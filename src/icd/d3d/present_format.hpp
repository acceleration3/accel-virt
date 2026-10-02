/* A surface format the kernel can present, both ways it is named: the DXGI
 * format a surface is made in, and the D3DDDIFORMAT the miniport's
 * descriptors scan it out as. Direct3D 9 names a back buffer by the second,
 * Direct3D 10 to 12 by the first; KernelSurface and the Direct3D 9 present
 * both cross through this one table. */
#ifndef AV_D3D_PRESENT_FORMAT_HPP
#define AV_D3D_PRESENT_FORMAT_HPP

#include <stdint.h>

namespace av::d3d {

class PresentFormat {
public:
    /* A surface of the DXGI format `dxgi`. */
    explicit PresentFormat(uint32_t dxgi);

    /* The DXGI format a Direct3D 9 surface of the D3DDDIFORMAT `ddi` is
     * presented in; false, and nothing changed, for one the kernel cannot
     * present. */
    [[nodiscard]] bool take_ddi(uint32_t ddi);
    uint32_t           dxgi() const;
    /* The D3DDDIFORMAT it is scanned out as: a format with no exact
     * counterpart as A8B8G8R8. */
    uint32_t           ddi() const;

private:
    uint32_t dxgi_;
};

}  // namespace av::d3d

#endif  // AV_D3D_PRESENT_FORMAT_HPP
