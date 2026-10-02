#include "executor/opencl/cl_shared_objects.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClSharedObjects::ClSharedObjects() : mems_{}, buffers_{}, memories_{}, count_(0u)
{
    AV_SCOPE("ClSharedObjects::ClSharedObjects");
}

bool ClSharedObjects::remember(uint64_t mem, uint32_t buffer, uint32_t memory)
{
    AV_SCOPE("ClSharedObjects::remember");
    if (mem == 0u || buffer == 0u || memory == 0u || count_ == max_objects) return false;
    for (uint32_t index = 0u; index < count_; index++)
        if (mems_[index] == mem) return false;
    mems_[count_] = mem;
    buffers_[count_] = buffer;
    memories_[count_] = memory;
    count_++;
    return true;
}

bool ClSharedObjects::forget(uint64_t mem, uint32_t &buffer, uint32_t &memory)
{
    AV_SCOPE("ClSharedObjects::forget");
    if (mem == 0u) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        if (mems_[index] != mem) continue;
        buffer = buffers_[index];
        memory = memories_[index];
        count_--;
        mems_[index] = mems_[count_];
        buffers_[index] = buffers_[count_];
        memories_[index] = memories_[count_];
        return true;
    }
    return false;
}

bool ClSharedObjects::take_any(uint64_t &mem, uint32_t &buffer, uint32_t &memory)
{
    AV_SCOPE("ClSharedObjects::take_any");
    if (count_ == 0u) return false;
    mem = mems_[count_ - 1u];
    return forget(mem, buffer, memory);
}

uint32_t ClSharedObjects::count() const
{
    AV_SCOPE("ClSharedObjects::count");
    return count_;
}

bool ClSharedObjects::invariants_hold() const
{
    AV_SCOPE("ClSharedObjects::invariants_hold");
    if (count_ > max_objects) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        if (mems_[index] == 0u || buffers_[index] == 0u || memories_[index] == 0u) return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (mems_[earlier] == mems_[index]) return false;
    }
    return true;
}

}  // namespace av
