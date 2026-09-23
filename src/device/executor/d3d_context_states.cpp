#include "device/executor/d3d_context_states.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t no_slot = D3dContextStates::capacity;

}  // namespace

D3dContextStates::D3dContextStates() : ids_{}, states_{}, count_(0u)
{
    AV_SCOPE("D3dContextStates::D3dContextStates");
}

uint32_t D3dContextStates::slot_of(uint32_t device) const
{
    AV_SCOPE("D3dContextStates::slot_of");
    for (uint32_t slot = 0u; slot < capacity; slot++)
        if (ids_[slot] == device) return slot;
    return no_slot;
}

bool D3dContextStates::add(uint32_t device, ID3DDeviceContextState *state)
{
    AV_SCOPE("D3dContextStates::add");
    if (device == 0u || state == nullptr || slot_of(device) != no_slot) return false;
    const uint32_t slot = slot_of(0u);
    if (slot == no_slot) return false;
    ids_[slot] = device;
    states_[slot] = state;
    count_++;
    return true;
}

ID3DDeviceContextState *D3dContextStates::find(uint32_t device) const
{
    AV_SCOPE("D3dContextStates::find");
    if (device == 0u) return nullptr;
    const uint32_t slot = slot_of(device);
    return slot == no_slot ? nullptr : states_[slot];
}

ID3DDeviceContextState *D3dContextStates::remove(uint32_t device)
{
    AV_SCOPE("D3dContextStates::remove");
    if (device == 0u) return nullptr;
    const uint32_t slot = slot_of(device);
    if (slot == no_slot) return nullptr;
    ID3DDeviceContextState *const state = states_[slot];
    ids_[slot] = 0u;
    states_[slot] = nullptr;
    count_--;
    return state;
}

ID3DDeviceContextState *D3dContextStates::remove_any()
{
    AV_SCOPE("D3dContextStates::remove_any");
    for (uint32_t slot = 0u; slot < capacity; slot++)
        if (ids_[slot] != 0u) return remove(ids_[slot]);
    return nullptr;
}

uint32_t D3dContextStates::count() const
{
    AV_SCOPE("D3dContextStates::count");
    return count_;
}

/* A kept id has a state and a free slot has neither; the count is the kept
 * ones, and no id is kept twice. */
bool D3dContextStates::invariants_hold() const
{
    AV_SCOPE("D3dContextStates::invariants_hold");
    uint32_t kept = 0u;
    for (uint32_t slot = 0u; slot < capacity; slot++) {
        if ((ids_[slot] == 0u) != (states_[slot] == nullptr)) return false;
        if (ids_[slot] == 0u) continue;
        kept++;
        for (uint32_t other = slot + 1u; other < capacity; other++)
            if (ids_[other] == ids_[slot]) return false;
    }
    return kept == count_;
}

}  // namespace av
