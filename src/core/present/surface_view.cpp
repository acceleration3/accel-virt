#include "core/present/surface_view.hpp"

#include "core/platform/platform.hpp"
#include "core/present/present_surface.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/resources/surface_layout.hpp"

namespace av {

SurfaceView::SurfaceView(Platform &platform, const PresentSurface &surface)
    : platform_(platform), surface_(surface), view_(nullptr), mapping_(nullptr)
{
    AV_SCOPE("SurfaceView::SurfaceView");
}

SurfaceView::~SurfaceView()
{
    AV_SCOPE("SurfaceView::~SurfaceView");
    if (view_ != nullptr) platform_.unmap_pages(view_, mapping_);
}

Status SurfaceView::map()
{
    AV_SCOPE("SurfaceView::map");
    view_ = surface_.map(platform_, mapping_);
    return view_ != nullptr ? Status::ok : Status::not_found;
}

uint8_t *SurfaceView::pixel(uint32_t x, uint32_t y) const
{
    AV_SCOPE("SurfaceView::pixel");
    const SurfaceLayout &layout = surface_.layout();
    return view_ + uint64_t{y} * layout.pitch() + uint64_t{x} * PresentSurface::pixel_bytes;
}

}  // namespace av
