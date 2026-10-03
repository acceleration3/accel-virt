/* The gate a Direct3D device holds its transport with after a present
 * (PublishGate): passing it signals a monitored fence behind everything the
 * device's kernel context holds - the runtime's wait for DWM to give back the
 * buffer drawn next among it - and waits on the CPU for that fence. */
#ifndef AV_UM_KERNEL_CONTEXT_GATE_HPP
#define AV_UM_KERNEL_CONTEXT_GATE_HPP

#include "guest/transport/publish_gate.hpp"
#include "icd/common/once_latch.hpp"
#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {

class KeyedLog;

}  // namespace av

namespace av::d3d {

class KernelContextGate final : public PublishGate {
public:
    KernelContextGate(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, HANDLE context, KeyedLog &lines);
    ~KernelContextGate();
    KernelContextGate(const KernelContextGate &) = delete;
    KernelContextGate &operator=(const KernelContextGate &) = delete;

    void pass() override;

private:
    [[nodiscard]] HRESULT make_fence();
    [[nodiscard]] HRESULT signal_and_wait();
    [[nodiscard]] HRESULT await_signalled();

    /* Longer than any frame DWM composes, short enough that a buffer nobody
     * gives back - a window nobody composes - costs a stutter and not a hang. */
    static constexpr DWORD bound_ms = 1000u;

    const D3DDDI_DEVICECALLBACKS &kernel_;
    HANDLE                        device_;
    HANDLE                        context_;
    KeyedLog                     &lines_;
    D3DKMT_HANDLE                 fence_;
    const volatile uint64_t      *reached_;
    uint64_t                      signalled_;
    HANDLE                        event_;
    icd::OnceLatch                refused_;
};

}  // namespace av::d3d

#endif  // AV_UM_KERNEL_CONTEXT_GATE_HPP
