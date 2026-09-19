#include "device/executor/gl_shared_objects.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/rvk_executor.h"

namespace av {

GlSharedObjects::GlSharedObjects() : kinds_(), names_(), count_(0u)
{
    AV_SCOPE("GlSharedObjects::GlSharedObjects");
}

uint32_t GlSharedObjects::space_of(uint32_t kind) const
{
    AV_SCOPE("GlSharedObjects::space_of");
    if (kind == RVK_SHARED_MEMORY) return RVK_SHARED_MEMORY;
    if (kind == RVK_SHARED_MONITORED_FENCE || kind == RVK_SHARED_NATIVE_FENCE) return RVK_SHARED_NATIVE_FENCE;
    return 0u;
}

bool GlSharedObjects::remember(uint32_t kind, uint32_t name)
{
    AV_SCOPE("GlSharedObjects::remember");
    const uint32_t space = space_of(kind);
    if (space == 0u || name == 0u || count_ == max_objects) return false;
    for (uint32_t i = 0u; i < count_; i++)
        if (names_[i] == name && kinds_[i] == space) return false;
    kinds_[count_] = space;
    names_[count_] = name;
    count_++;
    return true;
}

bool GlSharedObjects::forget(uint32_t kind, uint64_t wire_handle, uint32_t &name)
{
    AV_SCOPE("GlSharedObjects::forget");
    const uint32_t space = space_of(kind);
    const uint32_t wanted = static_cast<uint32_t>(wire_handle);
    if (space == 0u || wanted == 0u || wire_handle != wanted) return false;
    for (uint32_t i = 0u; i < count_; i++) {
        if (names_[i] != wanted || kinds_[i] != space) continue;
        name = wanted;
        count_--;
        kinds_[i] = kinds_[count_];
        names_[i] = names_[count_];
        return true;
    }
    return false;
}

bool GlSharedObjects::take_any(uint32_t &kind, uint32_t &name)
{
    AV_SCOPE("GlSharedObjects::take_any");
    if (count_ == 0u) return false;
    count_--;
    kind = kinds_[count_];
    name = names_[count_];
    return true;
}

uint32_t GlSharedObjects::count() const
{
    AV_SCOPE("GlSharedObjects::count");
    return count_;
}

bool GlSharedObjects::invariants_hold() const
{
    AV_SCOPE("GlSharedObjects::invariants_hold");
    if (count_ > max_objects) return false;
    for (uint32_t i = 0u; i < count_; i++) {
        if (names_[i] == 0u) return false;
        if (kinds_[i] != RVK_SHARED_MEMORY && kinds_[i] != RVK_SHARED_NATIVE_FENCE) return false;
        for (uint32_t j = i + 1u; j < count_; j++)
            if (names_[j] == names_[i] && kinds_[j] == kinds_[i]) return false;
    }
    return true;
}

}  // namespace av
