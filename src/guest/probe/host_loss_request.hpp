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

    [[nodiscard]] Status send();

private:
    static constexpr uint8_t  ring_order = 12u;
    static constexpr uint32_t reply_bytes = 4096u;
    static constexpr size_t   staging_bytes = 1024u;

    Kmt    &kmt_;
    uint8_t staging_[staging_bytes];
};

}

#endif
