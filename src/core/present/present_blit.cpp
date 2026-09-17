#include "core/present/present_blit.hpp"

#include "core/ddi/rect_record.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/commands/rectangle.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"

namespace av {

PresentBlit::PresentBlit(Platform &platform, const PresentSurface *source, const PresentSurface &destination)
    : platform_(platform), source_(source), destination_(destination), rows_(0u), clipped_rows_(0u)
{
    AV_SCOPE("PresentBlit::PresentBlit");
}

Status PresentBlit::blit(const Rectangle &source, const Rectangle &destination, const RectRecord *sub_rectangles,
                         uint32_t count)
{
    AV_SCOPE("PresentBlit::blit");
    platform_.require_irql_at_most(Irql::passive, "PresentBlit::blit");
    if (!reachable(false)) return Status::not_supported;
    const PresentArea from(source);
    const PresentArea into(destination);
    /* The desktop presents one to one; nothing here scales. */
    if (from.right() - from.left() != into.right() - into.left() ||
        from.bottom() - from.top() != into.bottom() - into.top())
        return Status::not_supported;
    if (count == 0u) return move_rectangle(from, into, false, 0u);
    if (sub_rectangles == nullptr) return Status::invalid_parameter;
    for (uint32_t index = 0u; index < count; index++) {
        const RectRecord &sub = sub_rectangles[index];
        const PresentArea piece = PresentArea(sub.left, sub.top, sub.right, sub.bottom)
                                      .intersected_with(into);
        /* The same piece of the source: the two rectangles are the same size. */
        const PresentArea piece_source(from.left() + piece.left() - into.left(),
                                       from.top() + piece.top() - into.top(),
                                       from.left() + piece.right() - into.left(),
                                       from.top() + piece.bottom() - into.top());
        const Status      moved = move_rectangle(piece_source, piece, false, 0u);
        if (moved != Status::ok) return moved;
    }
    return Status::ok;
}

Status PresentBlit::fill(uint32_t colour, const Rectangle &destination, const RectRecord *sub_rectangles,
                         uint32_t count)
{
    AV_SCOPE("PresentBlit::fill");
    platform_.require_irql_at_most(Irql::passive, "PresentBlit::fill");
    if (!reachable(true)) return Status::not_supported;
    const PresentArea into(destination);
    if (count == 0u) return move_rectangle(into, into, true, colour);
    if (sub_rectangles == nullptr) return Status::invalid_parameter;
    for (uint32_t index = 0u; index < count; index++) {
        const RectRecord &sub = sub_rectangles[index];
        const PresentArea piece = PresentArea(sub.left, sub.top, sub.right, sub.bottom)
                                      .intersected_with(into);
        const Status      filled = move_rectangle(piece, piece, true, colour);
        if (filled != Status::ok) return filled;
    }
    return Status::ok;
}

/* Every row of the rectangle, clipped to both surfaces before it moves. */
Status PresentBlit::move_rectangle(const PresentArea &source, const PresentArea &destination, bool filling,
                                   uint32_t colour)
{
    AV_SCOPE("PresentBlit::move_rectangle");
    const SurfaceLayout &into = destination_.layout();
    const SurfaceLayout &from = filling ? into : source_->layout();
    const PresentArea    written = destination.clipped_to(into.width(), into.height());
    for (int64_t row = written.top(); row < written.bottom(); row++) {
        const int64_t source_row = source.top() + row - destination.top();
        if (source_row < 0 || source_row >= static_cast<int64_t>(from.height())) continue;
        const int64_t     source_left = source.left() + written.left() - destination.left();
        const PresentArea read(source_left, source_row, source_left + written.width(), source_row + 1);
        const PresentArea taken = read.clipped_to(from.width(), from.height());
        if (taken.width() == 0) continue;
        const auto bytes = static_cast<uint32_t>(taken.width()) * pixel_bytes;
        const auto to_x = static_cast<uint32_t>(written.left() + (taken.left() - source_left));
        clipped_rows_++;
        const Status moved = move_row(static_cast<uint32_t>(taken.left()), static_cast<uint32_t>(source_row), to_x,
                                      static_cast<uint32_t>(row), bytes, filling, colour);
        if (moved != Status::ok) return moved;
        rows_++;
    }
    return Status::ok;
}

/* The bytes of one row, a page at a time on each side: an allocation's pages
 * are guest frames, and only one of them is mapped at a time. */
Status PresentBlit::move_row(uint32_t from_x, uint32_t from_y, uint32_t to_x, uint32_t to_y, uint32_t bytes,
                             bool filling, uint32_t colour)
{
    AV_SCOPE("PresentBlit::move_row");
    const SurfaceLayout &into = destination_.layout();
    uint64_t             to_offset = uint64_t{to_y} * into.pitch() + uint64_t{to_x} * pixel_bytes;
    uint64_t             from_offset = 0u;
    if (!filling) {
        const SurfaceLayout &from = source_->layout();
        from_offset = uint64_t{from_y} * from.pitch() + uint64_t{from_x} * pixel_bytes;
    }
    for (uint32_t moved = 0u; moved < bytes;) {
        uint32_t       into_page = 0u;
        uint32_t       into_within = 0u;
        uint8_t *const destination = map_at(destination_, to_offset + moved, into_within, into_page);
        if (destination == nullptr) return Status::not_found;
        uint32_t       from_page = bytes - moved;
        uint32_t       from_within = 0u;
        uint8_t *const source = filling ? nullptr : map_at(*source_, from_offset + moved, from_within, from_page);
        if (!filling && source == nullptr) {
            platform_.unmap_frame(destination);
            return Status::not_found;
        }
        const uint32_t room = into_page < from_page ? into_page : from_page;
        const uint32_t step = room < bytes - moved ? room : bytes - moved;
        uint8_t *const to = destination + into_within;
        if (filling)
            for (uint32_t at = 0u; at < step; at += pixel_bytes) platform_.copy(to + at, &colour, pixel_bytes);
        else
            platform_.copy(to, source + from_within, step);
        if (source != nullptr) platform_.unmap_frame(source);
        platform_.unmap_frame(destination);
        moved += step;
    }
    return Status::ok;
}

uint8_t *PresentBlit::map_at(const PresentSurface &surface, uint64_t offset, uint32_t &within, uint32_t &available)
{
    AV_SCOPE("PresentBlit::map_at");
    /* Every offset here is inside a surface `reachable` proved has a page for
     * every row, so the frame is one the allocation owns. */
    const uint64_t frame = surface.frame_at(offset, within);
    uint8_t *const view = platform_.map_frame(frame);
    if (view == nullptr) return nullptr;
    available = static_cast<uint32_t>(GuestPageList::page_bytes) - within;
    return view;
}

bool PresentBlit::reachable(bool filling) const
{
    AV_SCOPE("PresentBlit::reachable");
    if (!destination_.reachable()) return false;
    return filling ? true : source_ != nullptr && source_->reachable();
}

uint32_t PresentBlit::rows() const
{
    AV_SCOPE("PresentBlit::rows");
    return rows_;
}

bool PresentBlit::invariants_hold() const
{
    AV_SCOPE("PresentBlit::invariants_hold");
    return rows_ <= clipped_rows_;
}

}  // namespace av
