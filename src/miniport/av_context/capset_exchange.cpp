#include "miniport/av_context/capset_exchange.hpp"

#include "core/trace/function_scope.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "proto/context/capset_info.hpp"

namespace av {

CapsetExchange::CapsetExchange(Platform &platform, VirtioGpuDevice &device)
    : platform_(platform), device_(device), query_(platform), request_{}, answer_{}
    {
        AV_SCOPE("CapsetExchange::CapsetExchange");
    }

Status CapsetExchange::read(CapsetInfo &capset)
{
    AV_SCOPE("CapsetExchange::read");
    platform_.require_irql_at_most(Irql::passive, "CapsetExchange::read");
    while (!query_.complete()) {
        size_t       length = 0u;
        const Status asked = query_.next_request(request_, sizeof request_, length);
        if (asked != Status::ok) return asked;
        uint32_t     written = 0u;
        const Status answered = device_.ask_control(request_, length, answer_, sizeof answer_, written);
        if (answered != Status::ok) return answered;
        const Status judged = query_.accept(answer_, written);
        if (judged != Status::ok) return judged;
    }
    capset = query_.capset();
    return Status::ok;
}

}  // namespace av
