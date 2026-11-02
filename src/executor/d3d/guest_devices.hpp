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

    [[nodiscard]] Status add(uint32_t device, Handle *handle);
    [[nodiscard]] Status make_room();
    Handle              *find(uint32_t device) const;
    Handle              *remove(uint32_t device);
    Handle              *remove_any();
    uint32_t             count() const;

    Handle              *select(uint32_t device);
    Handle              *selected() const;
    uint32_t             selected_id() const;
    bool                 ready() const;

    void                 mark_removed(uint32_t device, uint32_t reason);
    uint32_t             removed_reason(uint32_t device) const;
    [[nodiscard]] bool   refuse(const CommandRefusal &refusal, const char *command, uint32_t reason);

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

}

#endif
