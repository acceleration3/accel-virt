#include "core/scheduling/context_loss.hpp"

#include "core/miniport/interrupt_notification.hpp"
#include "core/support/text_buffer.hpp"

namespace av {

ContextLoss::ContextLoss(Platform &platform)
    : platform_(platform), contexts_{}, fences_{}, whys_{}, told_{}, count_(0u) {}

void ContextLoss::lose(void *context, uint32_t fence, Status why)
{
    platform_.require_irql_at_most(Irql::dispatch, "ContextLoss::lose");
    for (uint32_t index = 0u; index < count_; index++)
        if (contexts_[index] == context) return;
    if (count_ == max_lost) return;
    contexts_[count_] = context;
    fences_[count_] = fence;
    whys_[count_] = why;
    told_[count_] = false;
    log(count_);
    count_++;
}

uint32_t ContextLoss::pending() const
{
    platform_.require_irql_at_most(Irql::dispatch, "ContextLoss::pending");
    uint32_t pending = 0u;
    for (uint32_t index = 0u; index < count_; index++) pending += told_[index] ? 0u : 1u;
    return pending;
}

Status ContextLoss::report(InterruptNotification &notification)
{
    platform_.require_irql_at_most(Irql::dispatch, "ContextLoss::report");
    for (uint32_t index = 0u; index < count_; index++) {
        if (told_[index]) continue;
        const Status told = notification.report_faulted(fences_[index], whys_[index]);
        if (told != Status::ok) return told;
        told_[index] = true;
    }
    return Status::ok;
}

void ContextLoss::log(uint32_t index) const
{
    char       bytes[96];
    TextBuffer line(bytes, sizeof bytes);
    line.append("rvkgpu: context 0x");
    line.append_hex(reinterpret_cast<uintptr_t>(contexts_[index]), 16u);
    line.append(" lost at fence ");
    line.append_decimal(fences_[index]);
    line.append(" status ");
    line.append_decimal(static_cast<uint64_t>(whys_[index]));
    platform_.log(line.text());
}

}  // namespace av
