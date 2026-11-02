#include "core/support/nt_status.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {
constexpr uint32_t mapping[] = {
    0x00000000u,
    0xC000000Du,
    0xC0000017u,
    0xC0000225u,
    0xC0000035u,
    0xC000009Au,
    0xC00000BBu,
    0xC0000184u,
    0xC0000023u,
    0xC000000Du,
};

}

NtStatus::NtStatus(Status status)
    : value_(static_cast<unsigned>(status) < sizeof mapping / sizeof mapping[0]
                 ? mapping[static_cast<unsigned>(status)]
                 : internal_error)
                 {
                     AV_SCOPE("NtStatus::NtStatus");
                 }

NtStatus::NtStatus(uint32_t value) : value_(value)
{
    AV_SCOPE("NtStatus::NtStatus");
}

NtStatus::NtStatus(int32_t answer) : value_(static_cast<uint32_t>(answer))
{
    AV_SCOPE("NtStatus::NtStatus");
}

int32_t NtStatus::value() const
{
    AV_SCOPE("NtStatus::value");
    return static_cast<int32_t>(value_);
}

bool NtStatus::succeeded() const
{
    AV_SCOPE("NtStatus::succeeded");
    return static_cast<int32_t>(value_) >= 0;
}

Status NtStatus::status() const
{
    AV_SCOPE("NtStatus::status");
    if (succeeded()) return Status::ok;
    for (unsigned index = 0u; index < sizeof mapping / sizeof mapping[0]; index++) {
        if (mapping[index] == value_) return static_cast<Status>(index);
    }
    return Status::invalid_state;
}

}
