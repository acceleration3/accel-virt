/* The device link one driver context holds on its process's IcdConnection,
 * from its make to its teardown, and the one place a faulted channel loses
 * the device behind it: every other context on the link finds out at its own
 * next call, on its own thread, which is what makes a sibling stop sending
 * rather than send into a device that is gone. OpenGL's and OpenCL's
 * contexts each hold one. */
#ifndef AV_ICD_CONTEXT_LINK_HPP
#define AV_ICD_CONTEXT_LINK_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class GuestConnection;
}  // namespace av

namespace av::icd {

class HostChannel;
class IcdConnection;
struct ContextLinkProbe;

class ContextLink {
public:
    explicit ContextLink(IcdConnection &connection);
    ~ContextLink();
    ContextLink(const ContextLink &) = delete;
    ContextLink &operator=(const ContextLink &) = delete;

    /* invalid_state when held; otherwise IcdConnection::acquire's answer,
     * holding nothing unless ok. */
    [[nodiscard]] Status acquire();
    /* Nothing when not held. */
    void                 release();
    bool                 held() const;
    /* The device the link names; only while held. */
    GuestConnection     &device() const;
    /* Whether the device is gone, losing it first when `channel` is lost
     * (HostChannel::lost). False when not held. */
    [[nodiscard]] bool   lost(const HostChannel &channel);
    /* The link's number, for a report; IcdConnection::no_link when not held. */
    uint32_t             link() const;
    /* Whether the link is on `connection`, held or not: which adapter a
     * driver context is made on. */
    bool                 on(const IcdConnection &connection) const;

private:
    friend struct ContextLinkProbe;

    IcdConnection &connection_;
    uint32_t       link_;
};

}  // namespace av::icd

#endif  // AV_ICD_CONTEXT_LINK_HPP
