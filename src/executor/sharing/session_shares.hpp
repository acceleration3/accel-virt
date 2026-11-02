#ifndef AV_SESSION_SHARES_HPP
#define AV_SESSION_SHARES_HPP

#include "core/support/status.hpp"
#include "executor/sharing/share_origin.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class SharedImageShape;

class HostShares;
class SessionBackend;

class SessionShares {
public:
    SessionShares();
    SessionShares(const SessionShares &) = delete;
    SessionShares &operator=(const SessionShares &) = delete;

    void        bind(HostShares &shares, uint32_t domain, SessionBackend &session);
    bool        bound() const;
    uint32_t    domain() const;
    HostShares *shares() const;

    [[nodiscard]] Status publish(uint64_t id, uint32_t kind, uint64_t size, uint64_t handle);
    [[nodiscard]] Status open(uint64_t id, uint32_t kind, uint64_t size, uint64_t into, uint64_t &handle);
    void                 close(uint64_t id, uint64_t handle);
    [[nodiscard]] Status publish_resource(uint64_t id, IUnknown *resource, ShareOrigin origin);
    void                 written(uint64_t id);
    bool                 describe_image(uint64_t id, const SharedImageShape &shape);
    void                 end();

private:
    SessionBackend *session_;
    HostShares     *shares_;
    uint32_t        domain_;
};

}

#endif
