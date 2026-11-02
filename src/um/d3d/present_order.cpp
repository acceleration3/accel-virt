#include "um/d3d/present_order.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "um/d3d/d3d_session.hpp"

namespace av::d3d {

namespace {
constexpr DWORD bound_ms = 1000u;

}

PresentOrder::PresentOrder(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, HANDLE context, D3dSession &session,
                           KeyedLog &lines)
    : context_(context), transport_(session.transport()), follower_(kernel, device, session.transport()),
      gate_(kernel, device, context, lines, bound_ms), shown_(session, lines)
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
    if (SUCCEEDED(status) && reuse == Reuse::after_context && !transport_.releases_by_packet())
        transport_.hold(gate_);
    return status;
}

HRESULT PresentOrder::flipped(HRESULT status, uint32_t source)
{
    AV_SCOPE("PresentOrder::flipped");
    if (SUCCEEDED(status)) shown_.flipped(source);
    return submitted(status, Reuse::after_context);
}

void PresentOrder::await_flip_shown()
{
    AV_SCOPE("PresentOrder::await_flip_shown");
    shown_.await();
}

}
