#ifndef AV_CLICD_CL_REPLICAS_HPP
#define AV_CLICD_CL_REPLICAS_HPP

#include "guest/kmt/avgpu_adapters.hpp"

#include <stdint.h>

namespace av::clicd {

class GuestClContext;

class ClReplicas {
public:
    static constexpr uint32_t most = AvgpuAdapters::max_adapters;

    struct Entry {
        GuestClContext *session;
        uint64_t        wire;
    };

    ClReplicas();
    ClReplicas(const ClReplicas &) = delete;
    ClReplicas &operator=(const ClReplicas &) = delete;

    [[nodiscard]] bool add(GuestClContext &session, uint64_t wire);
    uint64_t           in(const GuestClContext *session) const;
    uint32_t           count() const;
    const Entry       &at(uint32_t index) const;

    bool invariants_hold() const;

private:
    Entry    entries_[most];
    uint32_t count_;
};

}

#endif
