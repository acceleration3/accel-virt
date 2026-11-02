#ifndef AV_COUNTING_EVENT_OWNER_HPP
#define AV_COUNTING_EVENT_OWNER_HPP

#include "icd/opencl/cl_event_owner.hpp"

#include <cstdint>

namespace av {

class CountingEventOwner final : public clicd::ClEventOwner {
public:
    CountingEventOwner();

    void deliver_watched(clicd::ClObject &event) override;
    void release_watched(clicd::ClObject &event) override;

    std::uint32_t releases() const;
    std::uint32_t deliveries() const;
    bool          delivered_before_release() const;
    const clicd::ClObject *last() const;

private:
    const clicd::ClObject *last_;
    std::uint32_t          releases_;
    std::uint32_t          deliveries_;
    bool                   out_of_order_;
};

}

#endif
