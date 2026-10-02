/* One session's devices, by the id the guest named each with, the one whose
 * commands run now, and why each was removed.
 *
 * A guest process runs every device of one Direct3D runtime on one session,
 * so the doorbell, ring and arena a session costs are spent once per process
 * rather than once per device - the desktop's processes make dozens. What a
 * device owns on the host is a Handle: Direct3D 11's ID3DDeviceContextState,
 * which the session swaps in before running its commands, or a Direct3D 9
 * device. Each entry holds one reference, which the caller gives and takes
 * back; the table never touches the handles.
 *
 * A removed device stays kept, with the first reason it was removed for -
 * what went wrong; the rest follow from it - until the guest destroys it. It
 * is not ready: its commands are refused until the guest makes another. An id
 * the guest selected that names no device is removed the same way, until the
 * next selection: its commands are refused too, and the first refusal is the
 * one that says why rather than every one after it. */
#ifndef AV_GUEST_DEVICES_HPP
#define AV_GUEST_DEVICES_HPP

#include "core/trace/function_scope.hpp"
#include "executor/d3d/command_refusal.hpp"

#include <stdint.h>

namespace av {

struct GuestDevicesProbe;

template <typename Handle, uint32_t Capacity>
class GuestDevices {
public:
    /* The most devices one process runs at once. */
    static constexpr uint32_t capacity = Capacity;

    GuestDevices();
    GuestDevices(const GuestDevices &) = delete;
    GuestDevices &operator=(const GuestDevices &) = delete;

    /* Keeps `handle` under `device`. False for id 0, a null handle, an id
     * already kept, or a full table; the reference stays the caller's then. */
    [[nodiscard]] bool add(uint32_t device, Handle *handle);
    Handle            *find(uint32_t device) const;
    /* The handle kept under `device`, no longer kept, its reference passed to
     * the caller; a selected device is selected no more. Null for an id that
     * names nothing. */
    Handle            *remove(uint32_t device);
    /* Any kept handle, no longer kept, with its reference; null when none is.
     * How a closing session gives every device back. */
    Handle            *remove_any();
    uint32_t           count() const;

    /* Commands run on `device` from now on, and its handle; null for an id
     * that names nothing kept, which selects nothing until the next one. */
    Handle            *select(uint32_t device);
    /* The selected device's handle, null when none is; and its id, which an
     * id that named nothing keeps. */
    Handle            *selected() const;
    uint32_t           selected_id() const;
    /* A device is selected and not removed: its commands may run. */
    bool               ready() const;

    /* The device is removed for `reason` (an HRESULT, non-zero); the first
     * reason stands. Nothing for a device neither kept nor selected. */
    void               mark_removed(uint32_t device, uint32_t reason);
    /* Why the device was removed; 0 while it was not, or for one neither
     * kept nor selected. */
    uint32_t           removed_reason(uint32_t device) const;
    /* A command refused while the selected device spoke: that device removed
     * for `reason`, the first refusal logged (CommandRefusal). False when no
     * device was selected at all, which leaves the session nobody else to
     * blame. */
    [[nodiscard]] bool refuse(const CommandRefusal &refusal, const char *command, uint32_t reason);

    /* A kept id has a handle and a free slot has neither, nor a removal; the
     * count is the kept ones, no id is kept twice, the selected handle is the
     * selected id's, and only a selected id that names nothing kept has a
     * removal of its own. */
    bool invariants_hold() const;

private:
    friend struct GuestDevicesProbe;

    static constexpr uint32_t no_slot = Capacity;

    uint32_t slot_of(uint32_t device) const;

    uint32_t ids_[Capacity];
    Handle  *handles_[Capacity];
    uint32_t removed_[Capacity];
    uint32_t count_;
    uint32_t selected_id_;
    Handle  *selected_;
    /* Why the selected id was removed, when it names no device kept. */
    uint32_t selected_removed_;
};

template <typename Handle, uint32_t Capacity>
GuestDevices<Handle, Capacity>::GuestDevices()
    : ids_{}, handles_{}, removed_{}, count_(0u), selected_id_(0u), selected_(nullptr), selected_removed_(0u)
{
    AV_SCOPE("GuestDevices::GuestDevices");
}

template <typename Handle, uint32_t Capacity>
uint32_t GuestDevices<Handle, Capacity>::slot_of(uint32_t device) const
{
    AV_SCOPE("GuestDevices::slot_of");
    for (uint32_t slot = 0u; slot < Capacity; slot++)
        if (ids_[slot] == device) return slot;
    return no_slot;
}

template <typename Handle, uint32_t Capacity>
bool GuestDevices<Handle, Capacity>::add(uint32_t device, Handle *handle)
{
    AV_SCOPE("GuestDevices::add");
    if (device == 0u || handle == nullptr || slot_of(device) != no_slot) return false;
    const uint32_t slot = slot_of(0u);
    if (slot == no_slot) return false;
    ids_[slot] = device;
    handles_[slot] = handle;
    removed_[slot] = 0u;
    count_++;
    return true;
}

template <typename Handle, uint32_t Capacity>
Handle *GuestDevices<Handle, Capacity>::find(uint32_t device) const
{
    AV_SCOPE("GuestDevices::find");
    if (device == 0u) return nullptr;
    const uint32_t slot = slot_of(device);
    return slot == no_slot ? nullptr : handles_[slot];
}

template <typename Handle, uint32_t Capacity>
Handle *GuestDevices<Handle, Capacity>::remove(uint32_t device)
{
    AV_SCOPE("GuestDevices::remove");
    if (device == 0u) return nullptr;
    const uint32_t slot = slot_of(device);
    if (slot == no_slot) return nullptr;
    Handle *const handle = handles_[slot];
    ids_[slot] = 0u;
    handles_[slot] = nullptr;
    removed_[slot] = 0u;
    count_--;
    if (selected_id_ == device) {
        selected_id_ = 0u;
        selected_ = nullptr;
    }
    return handle;
}

template <typename Handle, uint32_t Capacity>
Handle *GuestDevices<Handle, Capacity>::remove_any()
{
    AV_SCOPE("GuestDevices::remove_any");
    for (uint32_t slot = 0u; slot < Capacity; slot++)
        if (ids_[slot] != 0u) return remove(ids_[slot]);
    return nullptr;
}

template <typename Handle, uint32_t Capacity>
uint32_t GuestDevices<Handle, Capacity>::count() const
{
    AV_SCOPE("GuestDevices::count");
    return count_;
}

template <typename Handle, uint32_t Capacity>
Handle *GuestDevices<Handle, Capacity>::select(uint32_t device)
{
    AV_SCOPE("GuestDevices::select");
    if (device != selected_id_) selected_removed_ = 0u;
    selected_id_ = device;
    selected_ = find(device);
    if (selected_ != nullptr) selected_removed_ = 0u;
    return selected_;
}

template <typename Handle, uint32_t Capacity>
Handle *GuestDevices<Handle, Capacity>::selected() const
{
    AV_SCOPE("GuestDevices::selected");
    return selected_;
}

template <typename Handle, uint32_t Capacity>
uint32_t GuestDevices<Handle, Capacity>::selected_id() const
{
    AV_SCOPE("GuestDevices::selected_id");
    return selected_id_;
}

template <typename Handle, uint32_t Capacity>
bool GuestDevices<Handle, Capacity>::ready() const
{
    AV_SCOPE("GuestDevices::ready");
    return selected_ != nullptr && removed_reason(selected_id_) == 0u;
}

template <typename Handle, uint32_t Capacity>
void GuestDevices<Handle, Capacity>::mark_removed(uint32_t device, uint32_t reason)
{
    AV_SCOPE("GuestDevices::mark_removed");
    if (device == 0u || reason == 0u) return;
    const uint32_t slot = slot_of(device);
    if (slot != no_slot) {
        if (removed_[slot] == 0u) removed_[slot] = reason;
        return;
    }
    if (device == selected_id_ && selected_removed_ == 0u) selected_removed_ = reason;
}

template <typename Handle, uint32_t Capacity>
uint32_t GuestDevices<Handle, Capacity>::removed_reason(uint32_t device) const
{
    AV_SCOPE("GuestDevices::removed_reason");
    if (device == 0u) return 0u;
    const uint32_t slot = slot_of(device);
    if (slot != no_slot) return removed_[slot];
    return device == selected_id_ ? selected_removed_ : 0u;
}

template <typename Handle, uint32_t Capacity>
bool GuestDevices<Handle, Capacity>::refuse(const CommandRefusal &refusal, const char *command, uint32_t reason)
{
    AV_SCOPE("GuestDevices::refuse");
    const uint32_t device = selected_id_;
    if (!refusal.removes(command, device, removed_reason(device) != 0u)) return false;
    mark_removed(device, reason);
    return true;
}

template <typename Handle, uint32_t Capacity>
bool GuestDevices<Handle, Capacity>::invariants_hold() const
{
    AV_SCOPE("GuestDevices::invariants_hold");
    uint32_t kept = 0u;
    for (uint32_t slot = 0u; slot < Capacity; slot++) {
        if ((ids_[slot] == 0u) != (handles_[slot] == nullptr)) return false;
        if (ids_[slot] == 0u && removed_[slot] != 0u) return false;
        if (ids_[slot] == 0u) continue;
        kept++;
        for (uint32_t other = slot + 1u; other < Capacity; other++)
            if (ids_[other] == ids_[slot]) return false;
    }
    if (selected_removed_ != 0u && (selected_id_ == 0u || selected_ != nullptr)) return false;
    return kept == count_ && selected_ == find(selected_id_);
}

}  // namespace av

#endif  // AV_GUEST_DEVICES_HPP
