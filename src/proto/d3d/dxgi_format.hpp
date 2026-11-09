#ifndef AV_DXGI_FORMAT_HPP
#define AV_DXGI_FORMAT_HPP

#include <stdint.h>

namespace av {
struct DxgiFormatRow;

class DxgiFormat {
public:
    static constexpr uint32_t count = 192u;

    explicit DxgiFormat(uint32_t format);

    bool     known() const;
    uint32_t value() const;

    uint32_t element_bytes() const;
    uint32_t element_width() const;
    uint32_t element_height() const;
    uint32_t planar_sample_bytes() const;

    uint32_t family() const;
    bool     typeless() const;
    bool     family_head() const;
    bool     depth() const;
    bool     srgb() const;
    uint32_t sibling() const;
    uint32_t resolved() const;

    uint32_t ddi_format() const;
    uint32_t vk_format() const;
    uint32_t vk_sibling() const;
    uint32_t cl_order() const;
    uint32_t cl_type() const;
    bool     cl_image_on_host() const;
    uint32_t drm_fourcc() const;
    bool     scanned_out() const;

private:
    uint32_t             format_;
    const DxgiFormatRow &row_;
};

}

#endif
