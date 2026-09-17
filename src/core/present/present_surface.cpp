#include "core/present/present_surface.hpp"

#include "core/trace/function_scope.hpp"

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

uint64_t PresentSurface::frame_at(uint64_t offset, uint32_t &within) const
{
    AV_SCOPE("PresentSurface::frame_at");
    const uint64_t page = offset / GuestPageList::page_bytes;
    within = static_cast<uint32_t>(offset % GuestPageList::page_bytes);
    if (page >= pages_.count()) return 0u;
    return pages_.frame(static_cast<uint32_t>(page));
}

bool PresentSurface::reachable() const
{
    AV_SCOPE("PresentSurface::reachable");
    if (!layout_.is_surface() || layout_.pitch() < layout_.width() * pixel_bytes) return false;
    if (!pages_.complete()) return false;
    return layout_.bytes() <= uint64_t{pages_.count()} * GuestPageList::page_bytes;
}

}  // namespace av
