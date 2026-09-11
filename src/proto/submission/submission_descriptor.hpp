/* One batch of ring bytes and the fence that completes it (docs/protocol.md). */
#ifndef AV_SUBMISSION_DESCRIPTOR_HPP
#define AV_SUBMISSION_DESCRIPTOR_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RingHeader;

class SubmissionDescriptor {
public:
    static constexpr size_t encoded_size = 32u;

    SubmissionDescriptor();
    SubmissionDescriptor(uint64_t fence_id, uint64_t begin, uint64_t end);

    uint64_t fence_id() const;
    uint64_t begin() const;
    uint64_t end() const;
    uint64_t length() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    /* A batch longer than the ring cannot have been written into it. */
    [[nodiscard]] Status validate_against(const RingHeader &ring) const;

private:
    static constexpr uint32_t magic = 0x534B5652u;  /* "RVKS" */

    uint64_t fence_id_;
    uint64_t begin_;
    uint64_t end_;
};

}  // namespace av

#endif  // AV_SUBMISSION_DESCRIPTOR_HPP
