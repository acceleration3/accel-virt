#include "guest/suite/guest_pattern.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t step = 7u;
/* A prime, so the sequence runs 251 bytes before it repeats. */
constexpr uint32_t period = 251u;

}  // namespace

GuestPattern::GuestPattern(uint32_t bytes) : bytes_(bytes)
{
    AV_SCOPE("GuestPattern::GuestPattern");
}

uint32_t GuestPattern::bytes() const
{
    AV_SCOPE("GuestPattern::bytes");
    return bytes_;
}

unsigned char GuestPattern::at(uint32_t offset) const
{
    AV_SCOPE("GuestPattern::at");
    return static_cast<unsigned char>(1u + offset % period * step % period);
}

void GuestPattern::write(volatile unsigned char *view) const
{
    AV_SCOPE("GuestPattern::write");
    for (uint32_t offset = 0u; offset < bytes_; offset++)
        view[offset] = at(offset);
}

void GuestPattern::clear(volatile unsigned char *view) const
{
    AV_SCOPE("GuestPattern::clear");
    for (uint32_t offset = 0u; offset < bytes_; offset++) view[offset] = 0u;
}

uint32_t GuestPattern::matched(const volatile unsigned char *view) const
{
    AV_SCOPE("GuestPattern::matched");
    uint32_t matched = 0u;
    for (uint32_t offset = 0u; offset < bytes_; offset++)
        matched += static_cast<uint32_t>(view[offset] == at(offset));
    return matched;
}

uint32_t GuestPattern::filled(const volatile unsigned char *view) const
{
    AV_SCOPE("GuestPattern::filled");
    uint32_t filled = 0u;
    for (uint32_t offset = 0u; offset < bytes_; offset++) filled += static_cast<uint32_t>(view[offset] != 0u);
    return filled;
}

}  // namespace av
