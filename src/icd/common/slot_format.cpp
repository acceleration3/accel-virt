#include "icd/common/slot_format.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/d3d/dxgi_format.hpp"

namespace av::icd {

namespace {

/* DXGI_FORMAT values. */
constexpr uint32_t dxgi_r8g8b8a8_unorm = 28u;
constexpr uint32_t dxgi_b8g8r8a8_unorm = 87u;
/* D3D11_BIND_SHADER_RESOURCE and D3D11_BIND_RENDER_TARGET. */
constexpr uint32_t bind_shader_resource = 0x8u;
constexpr uint32_t bind_render_target = 0x20u;
/* GL_RGBA8 and GL_SRGB8_ALPHA8. */
constexpr uint32_t gl_rgba8 = 0x8058u;
constexpr uint32_t gl_srgb8_alpha8 = 0x8C43u;
/* The slots a Vulkan image is copied into, top row first as an image holds
 * its rows. */
constexpr SlotLayout vulkan_layouts[] = {SlotLayout::bgra_top_down, SlotLayout::rgba_top_down};

}  // namespace

SlotFormat::SlotFormat(SlotLayout layout) : layout_(layout)
{
    AV_SCOPE("SlotFormat::SlotFormat");
}

bool SlotFormat::take_vk(uint32_t vk_format)
{
    AV_SCOPE("SlotFormat::take_vk");
    if (vk_format == 0u) return false;
    for (const SlotLayout candidate : vulkan_layouts) {
        const SlotFormat slot(candidate);
        if (vk_format != slot.vk_format() && vk_format != slot.vk_srgb()) continue;
        layout_ = candidate;
        return true;
    }
    return false;
}

SlotLayout SlotFormat::layout() const
{
    AV_SCOPE("SlotFormat::layout");
    return layout_;
}

uint32_t SlotFormat::dxgi() const
{
    AV_SCOPE("SlotFormat::dxgi");
    return bgra() ? dxgi_b8g8r8a8_unorm : dxgi_r8g8b8a8_unorm;
}

uint32_t SlotFormat::bind_flags() const
{
    AV_SCOPE("SlotFormat::bind_flags");
    return bind_shader_resource | bind_render_target;
}

uint32_t SlotFormat::vk_format() const
{
    AV_SCOPE("SlotFormat::vk_format");
    return DxgiFormat(dxgi()).vk_format();
}

uint32_t SlotFormat::vk_srgb() const
{
    AV_SCOPE("SlotFormat::vk_srgb");
    return DxgiFormat(dxgi()).vk_sibling();
}

bool SlotFormat::bgra() const
{
    AV_SCOPE("SlotFormat::bgra");
    return layout_ == SlotLayout::bgra_top_down;
}

uint32_t SlotFormat::gl_storage(bool srgb) const
{
    AV_SCOPE("SlotFormat::gl_storage");
    if (layout_ != SlotLayout::rgba_bottom_up) return 0u;
    return srgb ? gl_srgb8_alpha8 : gl_rgba8;
}

}  // namespace av::icd
