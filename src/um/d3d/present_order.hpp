#ifndef AV_UM_PRESENT_ORDER_HPP
#define AV_UM_PRESENT_ORDER_HPP

#include "um/d3d/batch_follower.hpp"
#include "um/d3d/flip_shown_wait.hpp"
#include "um/d3d/kernel_context_gate.hpp"

namespace av::d3d {

class D3dSession;

// Presents follow the submitted host work. Reused flip/primary buffers also
// keep subsequent host work behind the runtime's kernel-context waits, and a
// write into a fullscreen primary behind the flip before it (FlipShownWait).
class PresentOrder {
public:
    enum class Reuse { immediate, after_context };

    PresentOrder(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, HANDLE context, D3dSession &session,
                 KeyedLog &lines);
    ~PresentOrder();
    PresentOrder(const PresentOrder &) = delete;
    PresentOrder &operator=(const PresentOrder &) = delete;

    [[nodiscard]] HRESULT follow();
    [[nodiscard]] HRESULT submitted(HRESULT status, Reuse reuse);
    /* `status` of a present that flipped a fullscreen primary of VidPn
     * `source`, submitted as `submitted` does with after_context. */
    [[nodiscard]] HRESULT flipped(HRESULT status, uint32_t source);
    /* Before the host writes a fullscreen primary: returns once the flip
     * before it has been shown. */
    void                  await_flip_shown();

private:
    HANDLE context_;
    KmtTransport &transport_;
    BatchFollower follower_;
    KernelContextGate gate_;
    FlipShownWait shown_;
};

}  // namespace av::d3d

#endif  // AV_UM_PRESENT_ORDER_HPP
