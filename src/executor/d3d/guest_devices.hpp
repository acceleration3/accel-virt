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
 * one that says why rather than every one after it.
 *
 * A process may run any number of devices: the devices are KeyedSlots. */
#ifndef AV_GUEST_DEVICES_HPP
#define AV_GUEST_DEVICES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/command_refusal.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
struct GuestDevicesProbe;

template <typename Handle>
class GuestDevices {
public:
    explicit GuestDevices(BlockSource &blocks);
    GuestDevices(const GuestDevices &) = delete;
    GuestDevices &operator=(const GuestDevices &) = delete;

    /* Keeps `handle` under `device`: invalid_parameter for id 0 or a null
     * handle, conflict for an id already kept, no_memory when the table cannot
     * grow; the reference stays the caller's unless ok. */
    [[nodiscard]] Status add(uint32_t device, Handle *handle);
    /* Room for one more device, so the next add of a new id cannot fail:
     * asked before a device is made that could not be kept. */
    [[nodiscard]] Status make_room();
    Handle              *find(uint32_t device) const;
    /* The handle kept under `device`, no longer kept, its reference passed to
     * the caller; a selected device is selected no more. Null for an id that
     * names nothing. */
    Handle              *remove(uint32_t device);
    /* Any kept handle, no longer kept, with its reference; null when none is.
     * How a closing session gives every device back. */
    Handle              *remove_any();
    uint32_t             count() const;

    /* Commands run on `device` from now on, and its handle; null for an id
     * that names nothing kept, which selects nothing until the next one. */
    Handle              *select(uint32_t device);
    /* The selected device's handle, null when none is; and its id, which an
     * id that named nothing keeps. */
    Handle              *selected() const;
    uint32_t             selected_id() const;
    /* A device is selected and not removed: its commands may run. */
    bool                 ready() const;

    /* The device is removed for `reason` (an HRESULT, non-zero); the first
     * reason stands. Nothing for a device neither kept nor selected. */
    void                 mark_removed(uint32_t device, uint32_t reason);
    /* Why the device was removed; 0 while it was not, or for one neither
     * kept nor selected. */
    uint32_t             removed_reason(uint32_t device) const;
    /* A command refused while the selected device spoke: that device removed
     * for `reason`, the first refusal logged (CommandRefusal). False when no
     * device was selected at all, which leaves the session nobody else to
     * blame. */
    [[nodiscard]] bool   refuse(const CommandRefusal &refusal, const char *command, uint32_t reason);

    /* No kept id is 0 or has no handle; the selected handle is the selected
     * id's, and only a selected id that names nothing kept has a removal of
     * its own. */
    bool invariants_hold() const;

private:
    friend struct GuestDevicesProbe;

    struct Device {
        Handle  *handle_;
        uint32_t removed_;
    };

    KeyedSlots<uint32_t, Device> devices_;
    uint32_t                     selected_id_;
    Handle                      *selected_;
    /* Why the selected id was removed, when it names no device kept. */
    uint32_t                     selected_removed_;
};

template <typename Handle>
GuestDevices<Handle>::GuestDevices(BlockSource &blocks)
    : devices_(blocks, Pool::paged), selected_id_(0u), selected_(nullptr), selected_removed_(0u)
{
    AV_SCOPE("GuestDevices::GuestDevices");
}

template <typename Handle>
Status GuestDevices<Handle>::add(uint32_t device, Handle *handle)
{
    AV_SCOPE("GuestDevices::add");
    if (device == 0u || handle == nullptr) return Status::invalid_parameter;
    uint32_t slot = 0u;
    return devices_.insert(device, Device{handle, 0u}, slot);
}

template <typename Handle>
Status GuestDevices<Handle>::make_room()
{
    AV_SCOPE("GuestDevices::make_room");
    return devices_.reserve(devices_.count() + 1u);
}

template <typename Handle>
Handle *GuestDevices<Handle>::find(uint32_t device) const
{
    AV_SCOPE("GuestDevices::find");
    const Device *const kept = device == 0u ? nullptr : devices_.find(device);
    return kept == nullptr ? nullptr : kept->handle_;
}

template <typename Handle>
Handle *GuestDevices<Handle>::remove(uint32_t device)
{
    AV_SCOPE("GuestDevices::remove");
    Device kept{nullptr, 0u};
    if (device == 0u || !devices_.take(device, kept)) return nullptr;
    if (selected_id_ == device) {
        selected_id_ = 0u;
        selected_ = nullptr;
    }
    return kept.handle_;
}

template <typename Handle>
Handle *GuestDevices<Handle>::remove_any()
{
    AV_SCOPE("GuestDevices::remove_any");
    const uint32_t slot = devices_.next_live(0u);
    return slot == KeyedSlots<uint32_t, Device>::no_slot ? nullptr : remove(devices_.key_at(slot));
}

template <typename Handle>
uint32_t GuestDevices<Handle>::count() const
{
    AV_SCOPE("GuestDevices::count");
    return devices_.count();
}

template <typename Handle>
Handle *GuestDevices<Handle>::select(uint32_t device)
{
    AV_SCOPE("GuestDevices::select");
    if (device != selected_id_) selected_removed_ = 0u;
    selected_id_ = device;
    selected_ = find(device);
    if (selected_ != nullptr) selected_removed_ = 0u;
    return selected_;
}

template <typename Handle>
Handle *GuestDevices<Handle>::selected() const
{
    AV_SCOPE("GuestDevices::selected");
    return selected_;
}

template <typename Handle>
uint32_t GuestDevices<Handle>::selected_id() const
{
    AV_SCOPE("GuestDevices::selected_id");
    return selected_id_;
}

template <typename Handle>
bool GuestDevices<Handle>::ready() const
{
    AV_SCOPE("GuestDevices::ready");
    return selected_ != nullptr && removed_reason(selected_id_) == 0u;
}

template <typename Handle>
void GuestDevices<Handle>::mark_removed(uint32_t device, uint32_t reason)
{
    AV_SCOPE("GuestDevices::mark_removed");
    if (device == 0u || reason == 0u) return;
    Device *const kept = devices_.find(device);
    if (kept != nullptr) {
        if (kept->removed_ == 0u) kept->removed_ = reason;
        return;
    }
    if (device == selected_id_ && selected_removed_ == 0u) selected_removed_ = reason;
}

template <typename Handle>
uint32_t GuestDevices<Handle>::removed_reason(uint32_t device) const
{
    AV_SCOPE("GuestDevices::removed_reason");
    if (device == 0u) return 0u;
    const Device *const kept = devices_.find(device);
    if (kept != nullptr) return kept->removed_;
    return device == selected_id_ ? selected_removed_ : 0u;
}

template <typename Handle>
bool GuestDevices<Handle>::refuse(const CommandRefusal &refusal, const char *command, uint32_t reason)
{
    AV_SCOPE("GuestDevices::refuse");
    const uint32_t device = selected_id_;
    if (!refusal.removes(command, device, removed_reason(device) != 0u)) return false;
    mark_removed(device, reason);
    return true;
}

template <typename Handle>
bool GuestDevices<Handle>::invariants_hold() const
{
    AV_SCOPE("GuestDevices::invariants_hold");
    bool kept_whole = true;
    devices_.for_each([&kept_whole](uint32_t device, const Device &kept) {
        kept_whole = kept_whole && device != 0u && kept.handle_ != nullptr;
    });
    if (!kept_whole || !devices_.invariants_hold()) return false;
    if (selected_removed_ != 0u && (selected_id_ == 0u || selected_ != nullptr)) return false;
    return selected_ == find(selected_id_);
}

}  // namespace av

#endif  // AV_GUEST_DEVICES_HPP
