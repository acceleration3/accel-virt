#include "core/present/present_blit.hpp"

#include "core/ddi/rect_record.hpp"
#include "core/present/pixel_mover.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/commands/rectangle.hpp"
#include "proto/resources/surface_layout.hpp"

namespace av {

PresentBlit::PresentBlit(Platform &platform, PixelMover &mover, const PresentSurface *source,
                         const PresentSurface &destination)
    : platform_(platform), mover_(mover), source_(source), destination_(destination), filling_(false), colour_(0u),
      regions_{}, held_(0u), held_rows_(0u), rows_(0u)
{
    AV_SCOPE("PresentBlit::PresentBlit");
}

Status PresentBlit::blit(const Rectangle &source, const Rectangle &destination, const RectRecord *sub_rectangles,
                         uint32_t count)
{
    AV_SCOPE("PresentBlit::blit");
    platform_.require_irql_at_most(Irql::passive, "PresentBlit::blit");
    filling_ = false;
    if (source_ == nullptr || !reachable()) return Status::not_supported;
    const PresentArea from(source);
    const PresentArea into(destination);
    /* The desktop presents one to one; nothing here scales. */
    if (from.right() - from.left() != into.right() - into.left() ||
        from.bottom() - from.top() != into.bottom() - into.top())
        return Status::not_supported;
    return gather(from, into, sub_rectangles, count);
}

Status PresentBlit::fill(uint32_t colour, const Rectangle &destination, const RectRecord *sub_rectangles,
                         uint32_t count)
{
    AV_SCOPE("PresentBlit::fill");
    platform_.require_irql_at_most(Irql::passive, "PresentBlit::fill");
    filling_ = true;
    colour_ = colour;
    if (!reachable()) return Status::not_supported;
    const PresentArea into(destination);
    return gather(into, into, sub_rectangles, count);
}

Status PresentBlit::gather(const PresentArea &source, const PresentArea &destination,
                           const RectRecord *sub_rectangles, uint32_t count)
{
    AV_SCOPE("PresentBlit::gather");
    if (count != 0u && sub_rectangles == nullptr) return Status::invalid_parameter;
    Status added = count == 0u ? add(source, destination) : Status::ok;
    for (uint32_t index = 0u; index < count && added == Status::ok; index++) {
        const RectRecord &sub = sub_rectangles[index];
        const PresentArea piece = PresentArea(sub.left, sub.top, sub.right, sub.bottom).intersected_with(destination);
        /* The same piece of the source: the two rectangles are the same size. */
        const PresentArea piece_source(source.left() + piece.left() - destination.left(),
                                       source.top() + piece.top() - destination.top(),
                                       source.left() + piece.right() - destination.left(),
                                       source.top() + piece.bottom() - destination.top());
        added = add(piece_source, piece);
    }
    return added == Status::ok ? flush() : added;
}

/* The destination is clipped to its surface first, then the piece of the
 * source it maps to is clipped to the source's, and the destination follows
 * whatever that took away. */
Status PresentBlit::add(const PresentArea &source, const PresentArea &destination)
{
    AV_SCOPE("PresentBlit::add");
    const SurfaceLayout &into = destination_.layout();
    const SurfaceLayout &from = filling_ ? into : source_->layout();
    const PresentArea    kept = destination.clipped_to(into.width(), into.height());
    const int64_t        left = source.left() + kept.left() - destination.left();
    const int64_t        top = source.top() + kept.top() - destination.top();
    const PresentArea    taken =
        PresentArea(left, top, left + kept.width(), top + kept.height()).clipped_to(from.width(), from.height());
    if (taken.width() == 0 || taken.height() == 0) return Status::ok;
    const auto to_x = static_cast<uint32_t>(kept.left() + taken.left() - left);
    const auto to_y = static_cast<uint32_t>(kept.top() + taken.top() - top);
    regions_[held_++] = PresentRegion(static_cast<uint32_t>(taken.left()), static_cast<uint32_t>(taken.top()), to_x,
                                      to_y, static_cast<uint32_t>(taken.width()),
                                      static_cast<uint32_t>(taken.height()));
    held_rows_ += static_cast<uint32_t>(taken.height());
    return held_ == batch ? flush() : Status::ok;
}

Status PresentBlit::flush()
{
    AV_SCOPE("PresentBlit::flush");
    if (held_ == 0u) return Status::ok;
    const uint32_t count = held_;
    const uint32_t rows = held_rows_;
    held_ = 0u;
    held_rows_ = 0u;
    const Status moved = filling_ ? mover_.fill(destination_, colour_, regions_, count)
                                  : mover_.copy(*source_, destination_, regions_, count);
    if (moved == Status::ok) rows_ += rows;
    return moved;
}

bool PresentBlit::reachable() const
{
    AV_SCOPE("PresentBlit::reachable");
    if (!destination_.reachable()) return false;
    return filling_ || source_->reachable();
}

uint32_t PresentBlit::rows() const
{
    AV_SCOPE("PresentBlit::rows");
    return rows_;
}

bool PresentBlit::invariants_hold() const
{
    AV_SCOPE("PresentBlit::invariants_hold");
    const SurfaceLayout &into = destination_.layout();
    for (uint32_t index = 0u; index < held_; index++) {
        const PresentRegion &region = regions_[index];
        if (uint64_t{region.destination_x()} + region.width() > into.width() ||
            uint64_t{region.destination_y()} + region.height() > into.height())
            return false;
    }
    return held_ <= batch;
}

}  // namespace av
