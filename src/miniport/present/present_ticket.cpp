#include "miniport/present/present_ticket.hpp"

#include "core/platform/log_budget.hpp"
#include "core/platform/platform.hpp"
#include "miniport/present/present_queue.hpp"
#include "miniport/present/present_surface.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"

namespace av {

PresentTicket::PresentTicket(Platform &platform, LogBudget &budget, PresentQueue *queue, PixelMover &fallback,
                             const SurfacePoint &image)
    : platform_(platform), budget_(budget), queue_(queue), fallback_(fallback), image_(image), ticket_(0u)
{
    AV_SCOPE("PresentTicket::PresentTicket");
}

Status PresentTicket::copy(const PresentSurface &source, const PresentSurface &destination,
                           const PresentRegion *regions, uint32_t count)
{
    AV_SCOPE("PresentTicket::copy");
    if (queued(source.blt_into(destination, count), &source, destination, regions)) return Status::ok;
    return fallback_.copy(source, destination, regions, count);
}

Status PresentTicket::fill(const PresentSurface &destination, uint32_t colour, const PresentRegion *regions,
                           uint32_t count)
{
    AV_SCOPE("PresentTicket::fill");
    if (queued(destination.fill_with(colour, count), nullptr, destination, regions)) return Status::ok;
    return fallback_.fill(destination, colour, regions, count);
}

bool PresentTicket::queued(const PresentCopyRequest &request, const PresentSurface *source,
                           const PresentSurface &destination, const PresentRegion *regions)
{
    AV_SCOPE("PresentTicket::queued");
    if (queue_ == nullptr) return false;
    uint64_t     ticket = 0u;
    const bool   imaged = image_.names() && source != nullptr;
    const Status status = imaged ? queue_->queue_image(image_, request, destination, regions, ticket)
                                 : queue_->queue(request, source, destination, regions, ticket);
    if (status == Status::ok) {
        ticket_ = ticket;
        return true;
    }
    log_refusal(status);
    return false;
}

void PresentTicket::log_refusal(Status status)
{
    AV_SCOPE("PresentTicket::log_refusal");
    if (!budget_.allows()) return;
    char       bytes[96];
    TextBuffer message(bytes, sizeof bytes);
    message.append("avgpu: the present engine did not take a present, status ");
    message.append_decimal(static_cast<uint32_t>(status));
    message.append("; it moves now");
    platform_.log(message.text());
}

uint64_t PresentTicket::ticket() const
{
    AV_SCOPE("PresentTicket::ticket");
    return ticket_;
}

}
