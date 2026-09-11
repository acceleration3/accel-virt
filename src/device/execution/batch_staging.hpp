/* The memory one context's worker executes from: the copy of each batch's
 * payload, never larger than the ring's data area, and the executor's answer,
 * never larger than the reply slot. Allocated at setup, released with the
 * context, so the worker never allocates. */
#ifndef AV_BATCH_STAGING_HPP
#define AV_BATCH_STAGING_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class Host;
struct BatchStagingProbe;

class BatchStaging {
public:
    /* The largest answer staged: a ring's largest data area. */
    static constexpr uint64_t max_reply = uint64_t{1} << 24;

    explicit BatchStaging(Host &host);
    ~BatchStaging();
    BatchStaging(const BatchStaging &) = delete;
    BatchStaging &operator=(const BatchStaging &) = delete;

    /* Main loop. A reply capacity of 0 takes no reply block. no_memory,
     * holding nothing, when the host has none; invalid_state once holding. */
    [[nodiscard]] Status allocate(uint64_t payload_capacity, uint64_t reply_capacity);
    /* Main loop. */
    void                 release();
    uint8_t             *payload() const;
    size_t               payload_capacity() const;
    uint8_t             *reply() const;
    size_t               reply_capacity() const;

    bool invariants_hold() const;

private:
    Host    &host_;
    uint8_t *payload_;
    size_t   payload_capacity_;
    uint8_t *reply_;
    size_t   reply_capacity_;

    friend struct BatchStagingProbe;
};

}  // namespace av

#endif  // AV_BATCH_STAGING_HPP
