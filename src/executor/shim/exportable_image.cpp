#include "executor/shim/exportable_image.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {
constexpr uint64_t drm_format_mod_linear = 0u;
constexpr uint64_t drm_format_mod_invalid = 0x00ffffffffffffffull;

}

ExportableImage::ExportableImage(ExportableImages &owner)
    : TextureHold(owner), image_(), modifier_(drm_format_mod_invalid)
{
    AV_SCOPE("ExportableImage::ExportableImage");
}

VulkanImage &ExportableImage::image()
{
    AV_SCOPE("ExportableImage::image");
    return image_;
}

const VulkanImage &ExportableImage::image() const
{
    AV_SCOPE("ExportableImage::image");
    return image_;
}

void ExportableImage::laid_out_by(uint64_t modifier)
{
    AV_SCOPE("ExportableImage::laid_out_by");
    modifier_ = modifier;
}

uint64_t ExportableImage::modifier() const
{
    AV_SCOPE("ExportableImage::modifier");
    return modifier_;
}

bool ExportableImage::rows() const
{
    AV_SCOPE("ExportableImage::rows");
    return modifier_ == drm_format_mod_linear;
}

void ExportableImage::free()
{
    AV_SCOPE("ExportableImage::free");
    image_.release();
}

}
