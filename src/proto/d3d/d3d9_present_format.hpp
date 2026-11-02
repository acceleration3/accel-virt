#ifndef AV_D3D9_PRESENT_FORMAT_HPP
#define AV_D3D9_PRESENT_FORMAT_HPP

#include <stdint.h>

namespace av {

class D3d9PresentFormat {
public:
    explicit D3d9PresentFormat(uint32_t dxgi);

    [[nodiscard]] bool take_ddi(uint32_t ddi);
    uint32_t           dxgi() const;
    uint32_t           ddi() const;
    uint32_t           viewed() const;

private:
    uint32_t dxgi_;
};

}

#endif
