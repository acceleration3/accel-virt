/* The private data of a packet a process submits on one of its ordinary
 * contexts to follow a batch on one of its av contexts: the av context is
 * named by its ring's allocation handle, which only resolves in that process,
 * and the packet completes once the host has completed `fence` on it.
 *
 * What a Direct3D 12 queue's context is given after an execute: the runtime
 * signals an application's fence on that context once its packets are done,
 * and the work went through the ring, not the context (docs/protocol.md). */
#ifndef AV_FOLLOW_RECORD_HPP
#define AV_FOLLOW_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class FollowRecord {
public:
    static constexpr size_t encoded_size = 16u;

    FollowRecord();
    FollowRecord(uint32_t ring, uint64_t fence);

    uint32_t ring() const;
    uint64_t fence() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00465641u;  /* "AVF" */

    uint32_t ring_;
    uint64_t fence_;
};

}  // namespace av

#endif  // AV_FOLLOW_RECORD_HPP
