/* A probe asking the host to lose its GPU's Direct3D 11 device, as a GPU
 * fault would: a Direct3D context of its own on the first avgpu adapter - the
 * one a device made on the default adapter runs on - sends LOSE_HOST_DEVICE
 * (HostLoss) and goes. The host loses one only where it was started with the
 * layer that does it (tests/gpu_fault); every other host keeps it. */
#ifndef AV_HOST_LOSS_REQUEST_HPP
#define AV_HOST_LOSS_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class Kmt;

class HostLossRequest {
public:
    explicit HostLossRequest(Kmt &kmt);
    HostLossRequest(const HostLossRequest &) = delete;
    HostLossRequest &operator=(const HostLossRequest &) = delete;

    /* ok when the host lost the device; not_supported when it kept it; the
     * adapter's, the connection's or the context's own refusal otherwise,
     * with nothing left open. */
    [[nodiscard]] Status send();

private:
    static constexpr uint8_t  ring_order = 12u;
    static constexpr uint32_t reply_bytes = 4096u;
    static constexpr size_t   staging_bytes = 1024u;

    Kmt    &kmt_;
    uint8_t staging_[staging_bytes];
};

}  // namespace av

#endif  // AV_HOST_LOSS_REQUEST_HPP
