#ifndef AV_UM_KERNEL_CONTEXT_GATE_HPP
#define AV_UM_KERNEL_CONTEXT_GATE_HPP

#include "guest/transport/publish_gate.hpp"
#include "icd/common/once_latch.hpp"
#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {

class KeyedLog;

}

namespace av::d3d {

class KernelContextGate final : public PublishGate {
public:
    KernelContextGate(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, HANDLE context, KeyedLog &lines,
                      DWORD bound_ms);
    ~KernelContextGate();
    KernelContextGate(const KernelContextGate &) = delete;
    KernelContextGate &operator=(const KernelContextGate &) = delete;

    void pass() override;
    uint32_t stalled() const;

private:
    [[nodiscard]] HRESULT make_fence();
    [[nodiscard]] HRESULT signal_and_wait();
    [[nodiscard]] HRESULT await_signalled();

    const D3DDDI_DEVICECALLBACKS &kernel_;
    HANDLE                        device_;
    HANDLE                        context_;
    KeyedLog                     &lines_;
    DWORD                         bound_ms_;
    uint32_t                      stalled_;
    D3DKMT_HANDLE                 fence_;
    const volatile uint64_t      *reached_;
    uint64_t                      signalled_;
    HANDLE                        event_;
    icd::OnceLatch                refused_;
};

}

#endif
