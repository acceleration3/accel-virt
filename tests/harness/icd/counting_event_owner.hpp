/* The ClEventOwner a watcher test watches.
 *
 * The real owner is ClIcdRoot, whose release_watched drops a reference in the
 * object table; a watcher test has no table and no root, and what it needs to
 * say is narrower: that every delivered registration gave its event back,
 * exactly once, after the callback rather than before. This records both. */
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
    /* Deliveries, and that each one came before its release. */
    std::uint32_t deliveries() const;
    bool          delivered_before_release() const;
    /* The last event given back, so a test can say which one it was. */
    const clicd::ClObject *last() const;

private:
    const clicd::ClObject *last_;
    std::uint32_t          releases_;
    std::uint32_t          deliveries_;
    bool                   out_of_order_;
};

}  // namespace av

#endif  // AV_COUNTING_EVENT_OWNER_HPP
