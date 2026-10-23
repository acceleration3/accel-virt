#include "um/d3d/present_order.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"

namespace av::d3d {

namespace {

/* Longer than any frame DWM composes, short enough that a buffer nobody
 * gives back - a window nobody composes - costs a stutter and not a hang. */
constexpr DWORD bound_ms = 1000u;

}  // namespace

PresentOrder::PresentOrder(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, HANDLE context,
                           KmtTransport &transport, KeyedLog &lines)
    : context_(context), transport_(transport), follower_(kernel, device, transport),
      gate_(kernel, device, context, lines, bound_ms)
{
    AV_SCOPE("PresentOrder::PresentOrder");
}

PresentOrder::~PresentOrder()
{
    AV_SCOPE("PresentOrder::~PresentOrder");
    transport_.forget(gate_);
}

HRESULT PresentOrder::follow()
{
    AV_SCOPE("PresentOrder::follow");
    return follower_.follow(context_);
}

HRESULT PresentOrder::submitted(HRESULT status, Reuse reuse)
{
    AV_SCOPE("PresentOrder::submitted");
    /* A transport that releases its batches by packet has them queued on the
     * context behind the present, and its reuse wait with it: the next batch
     * is not published before the scheduler has run both, so no CPU wait. */
    if (SUCCEEDED(status) && reuse == Reuse::after_context && !transport_.releases_by_packet())
        transport_.hold(gate_);
    return status;
}

}  // namespace av::d3d
