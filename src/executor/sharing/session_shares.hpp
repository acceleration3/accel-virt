/* The shares one session reaches: its GPU's HostShares, in the domain - the
 * guest device - its context belongs to, as that session. What a backend and
 * the Direct3D sessions inside it call, so none of them carries the three
 * apart. One per session slot, bound as a context opens there and ended as
 * it closes; unbound, it shares nothing and opens nothing. */
#ifndef AV_SESSION_SHARES_HPP
#define AV_SESSION_SHARES_HPP

#include "core/support/status.hpp"
#include "executor/sharing/share.hpp"

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

    /* `shares` in `domain`, as `session`, from now on. */
    void        bind(HostShares &shares, uint32_t domain, SessionBackend &session);
    bool        bound() const;
    uint32_t    domain() const;
    HostShares *shares() const;

    [[nodiscard]] Status publish(uint64_t id, uint32_t kind, uint64_t size, uint64_t handle);
    [[nodiscard]] Status open(uint64_t id, uint32_t kind, uint64_t size, uint64_t &handle);
    void                 close(uint64_t id);
    [[nodiscard]] Status publish_resource(uint64_t id, IUnknown *resource, ShareOrigin origin);
    void                 written(uint64_t id);
    /* HostShares::describe_image in this session's domain. */
    bool                 describe_image(uint64_t id, const SharedImageShape &shape);
    /* Every hold of this session let go of (HostShares::session_closed), and
     * unbound. */
    void                 end();

private:
    SessionBackend *session_;
    HostShares     *shares_;
    uint32_t        domain_;
};

}  // namespace av

#endif  // AV_SESSION_SHARES_HPP
