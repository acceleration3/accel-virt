#include "nt_status.hpp"

namespace av {

namespace {

/* Indexed by Status. */
constexpr uint32_t mapping[] = {
    0x00000000u,  /* ok                          STATUS_SUCCESS */
    0xC000000Du,  /* invalid_parameter           STATUS_INVALID_PARAMETER */
    0xC0000017u,  /* no_memory                   STATUS_NO_MEMORY */
    0xC0000225u,  /* not_found                   STATUS_NOT_FOUND */
    0xC0000035u,  /* conflict                    STATUS_OBJECT_NAME_COLLISION */
    0xC000009Au,  /* no_space                    STATUS_INSUFFICIENT_RESOURCES */
    0xC00000BBu,  /* not_supported               STATUS_NOT_SUPPORTED */
    0xC0000184u,  /* invalid_state               STATUS_INVALID_DEVICE_STATE */
    0xC0000023u,  /* too_large                   STATUS_BUFFER_TOO_SMALL */
    0xC000000Du,  /* malformed                   STATUS_INVALID_PARAMETER */
};

}  // namespace

NtStatus::NtStatus(Status status)
    : value_(static_cast<unsigned>(status) < sizeof mapping / sizeof mapping[0]
                 ? mapping[static_cast<unsigned>(status)]
                 : internal_error) {}

NtStatus::NtStatus(uint32_t value) : value_(value) {}

NtStatus::NtStatus(int32_t answer) : value_(static_cast<uint32_t>(answer)) {}

int32_t NtStatus::value() const { return static_cast<int32_t>(value_); }

bool NtStatus::succeeded() const { return static_cast<int32_t>(value_) >= 0; }

/* Status::malformed shares STATUS_INVALID_PARAMETER and comes back as
 * invalid_parameter, the first entry with that value. */
Status NtStatus::status() const
{
    if (succeeded()) return Status::ok;
    for (unsigned index = 0u; index < sizeof mapping / sizeof mapping[0]; index++) {
        if (mapping[index] == value_) return static_cast<Status>(index);
    }
    return Status::invalid_state;
}

}  // namespace av
