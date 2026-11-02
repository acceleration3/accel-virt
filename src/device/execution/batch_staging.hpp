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
    static constexpr uint64_t max_reply = uint64_t{1} << 24;

    explicit BatchStaging(Host &host);
    ~BatchStaging();
    BatchStaging(const BatchStaging &) = delete;
    BatchStaging &operator=(const BatchStaging &) = delete;

    [[nodiscard]] Status allocate(uint64_t payload_capacity, uint64_t reply_capacity);
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

}

#endif
