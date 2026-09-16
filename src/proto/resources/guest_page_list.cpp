#include "proto/resources/guest_page_list.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GuestPageList::GuestPageList() : frames_(nullptr), count_(0u)
{
    AV_SCOPE("GuestPageList::GuestPageList");
}

GuestPageList::GuestPageList(const uint64_t *frames, uint32_t count)
    : frames_(frames), count_(frames != nullptr ? count : 0u)
    {
        AV_SCOPE("GuestPageList::GuestPageList");
    }

uint32_t GuestPageList::count() const
{
    AV_SCOPE("GuestPageList::count");
    return count_;
}

uint64_t GuestPageList::frame(uint32_t index) const
{
    AV_SCOPE("GuestPageList::frame");
    return index < count_ ? frames_[index] : 0u;
}

bool GuestPageList::complete() const
{
    AV_SCOPE("GuestPageList::complete");
    for (uint32_t index = 0u; index < count_; index++)
        if (frames_[index] == 0u || (frames_[index] & (page_bytes - 1u)) != 0u) return false;
    return count_ != 0u;
}

uint32_t GuestPageList::run_length(uint32_t index) const
{
    AV_SCOPE("GuestPageList::run_length");
    if (index >= count_) return 0u;
    uint32_t length = 1u;
    while (index + length < count_ && frames_[index + length] == frames_[index + length - 1u] + page_bytes) length++;
    return length;
}

uint32_t GuestPageList::runs() const
{
    AV_SCOPE("GuestPageList::runs");
    uint32_t runs = 0u;
    for (uint32_t index = 0u; index < count_; index += run_length(index)) runs++;
    return runs;
}

}  // namespace av
