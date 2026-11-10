#ifndef AV_CLICD_CL_CONTEXT_GROUP_HPP
#define AV_CLICD_CL_CONTEXT_GROUP_HPP

#include "guest/kmt/avgpu_adapters.hpp"

#include <stdint.h>

namespace av::clicd {

class ClDevice;
class ClObject;
class GuestClContext;

class ClContextGroup {
public:
    static constexpr uint32_t most = AvgpuAdapters::max_adapters;

    struct Member {
        GuestClContext *session;
        ClDevice       *device;
        uint64_t        host_context;
        uint64_t        copy_queue;
        uint64_t        done_event;
    };

    ClContextGroup();
    ClContextGroup(const ClContextGroup &) = delete;
    ClContextGroup &operator=(const ClContextGroup &) = delete;

    [[nodiscard]] bool add(GuestClContext &session, ClDevice &device, uint64_t host_context);
    void               bind(ClObject &owner);

    ClObject     &owner() const;
    uint32_t      count() const;
    Member       &at(uint32_t index);
    const Member &at(uint32_t index) const;
    Member       *of(const GuestClContext *session);
    Member       *of(const ClDevice *device);
    bool          has(const GuestClContext *session) const;

    bool invariants_hold() const;

private:
    ClObject *owner_;
    Member    members_[most];
    uint32_t  count_;
};

}

#endif
