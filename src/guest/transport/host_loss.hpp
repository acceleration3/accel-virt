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

    [[nodiscard]] Status send();

private:
    ClientTransport &transport_;
};

}

#endif
