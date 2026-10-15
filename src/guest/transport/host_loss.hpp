/* A test's GPU fault: the host device a context runs on made lost
 * (LOSE_HOST_DEVICE, docs/protocol.md "Losing the host device"), as a fault
 * of the host GPU would lose it. The host loses one only where it was started
 * with the layer that loses devices on request; anywhere else it answers that
 * it kept it, and nothing happens.
 *
 * The context that asks is not faulted by the answer: like every context on
 * the lost device it is faulted when the device's worker next polls, so what
 * a test sees afterwards is what a fault the host found by itself does. */
#ifndef AV_HOST_LOSS_HPP
#define AV_HOST_LOSS_HPP

#include "core/support/status.hpp"

namespace av {

class ClientTransport;

class HostLoss {
public:
    explicit HostLoss(ClientTransport &transport);
    HostLoss(const HostLoss &) = delete;
    HostLoss &operator=(const HostLoss &) = delete;

    /* ok when the host lost the device; not_supported when it kept it;
     * invalid_state when the call failed - the context already faulted; and
     * malformed for an answer that is not one. */
    [[nodiscard]] Status send();

private:
    ClientTransport &transport_;
};

}  // namespace av

#endif  // AV_HOST_LOSS_HPP
