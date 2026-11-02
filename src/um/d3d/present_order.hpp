#ifndef AV_UM_PRESENT_ORDER_HPP
#define AV_UM_PRESENT_ORDER_HPP

#include "um/d3d/batch_follower.hpp"
#include "um/d3d/kernel_context_gate.hpp"

namespace av::d3d {

class PresentOrder {
public:
    enum class Reuse { immediate, after_context };

    PresentOrder(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, HANDLE context,
                 KmtTransport &transport, KeyedLog &lines);
    ~PresentOrder();
    PresentOrder(const PresentOrder &) = delete;
    PresentOrder &operator=(const PresentOrder &) = delete;

    [[nodiscard]] HRESULT follow();
    [[nodiscard]] HRESULT submitted(HRESULT status, Reuse reuse);

private:
    HANDLE context_;
    KmtTransport &transport_;
    BatchFollower follower_;
    KernelContextGate gate_;
};

}

#endif
