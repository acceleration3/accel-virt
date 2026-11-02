#ifndef AV_ICD_CONTEXT_LINK_HPP
#define AV_ICD_CONTEXT_LINK_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class GuestConnection;
}

namespace av::icd {

class DeviceLink;
class HostChannel;
class IcdConnection;
struct ContextLinkProbe;

class ContextLink {
public:
    explicit ContextLink(IcdConnection &connection);
    ~ContextLink();
    ContextLink(const ContextLink &) = delete;
    ContextLink &operator=(const ContextLink &) = delete;

    [[nodiscard]] Status acquire();
    void                 release();
    bool                 held() const;
    GuestConnection     &device() const;
    [[nodiscard]] bool   lost(const HostChannel &channel);
    bool                 on(const IcdConnection &connection) const;

private:
    friend struct ContextLinkProbe;

    IcdConnection &connection_;
    DeviceLink    *link_;
};

}

#endif
