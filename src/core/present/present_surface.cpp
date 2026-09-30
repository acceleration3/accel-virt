#include "core/present/present_surface.hpp"

#include "core/platform/platform.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"

namespace av {

PresentSurface::PresentSurface(const SurfaceLayout &layout, const GuestPageList &pages)
    : layout_(layout), pages_(pages)
{
    AV_SCOPE("PresentSurface::PresentSurface");
}

const SurfaceLayout &PresentSurface::layout() const
{
    AV_SCOPE("PresentSurface::layout");
    return layout_;
}

uint8_t *PresentSurface::map(Platform &platform, void *&mapping) const
{
    AV_SCOPE("PresentSurface::map");
    return platform.map_pages(pages_, mapping);
}

bool PresentSurface::reachable() const
{
    AV_SCOPE("PresentSurface::reachable");
    if (!layout_.is_surface() || layout_.pitch() < layout_.width() * pixel_bytes) return false;
    /* A pitch of whole pixels keeps every row's bytes four-aligned, so no pixel
     * straddles a page and a fill never writes past the page it mapped. */
    if (layout_.pitch() % pixel_bytes != 0u) return false;
    if (!pages_.complete()) return false;
    return layout_.bytes() <= uint64_t{pages_.count()} * GuestPageList::page_bytes;
}

bool PresentSurface::swaps_with(const PresentSurface &other) const
{
    AV_SCOPE("PresentSurface::swaps_with");
    return layout_.red_first() != other.layout_.red_first();
}

uint32_t PresentSurface::runs() const
{
    AV_SCOPE("PresentSurface::runs");
    return pages_.runs();
}

void PresentSurface::write_runs(WireWriter &writer) const
{
    AV_SCOPE("PresentSurface::write_runs");
    pages_.write_runs(writer);
}

PresentCopyRequest PresentSurface::blt_into(const PresentSurface &destination, uint32_t regions) const
{
    AV_SCOPE("PresentSurface::blt_into");
    return PresentCopyRequest(PresentOperation::blt, swaps_with(destination) ? PresentCopyRequest::flag_swap_red_blue : 0u,
                              0u, regions, runs(), layout_.pitch(), destination.runs(), destination.layout().pitch());
}

PresentCopyRequest PresentSurface::fill_with(uint32_t colour, uint32_t regions) const
{
    AV_SCOPE("PresentSurface::fill_with");
    return PresentCopyRequest(PresentOperation::color_fill, 0u, colour, regions, 0u, 0u, runs(), layout_.pitch());
}

}  // namespace av
