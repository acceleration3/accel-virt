/* One source or target mode set of a VidPn, held from acquire() or create()
 * until assign() hands it to the VidPn or the destructor releases it. Every
 * mode info it acquires is released before the call that acquired it returns.
 * All of it is PASSIVE. */
#ifndef AV_VIDPN_MODE_SET_HPP
#define AV_VIDPN_MODE_SET_HPP

#include "core/ddi/dxgk_vidpn_interface_records.hpp"
#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/status.hpp"
#include "core/vidpn/vidpn.hpp"

#include <stdint.h>

namespace av {

template <typename Mode, typename ModeSetInterface>
class VidPnModeSet {
public:
    VidPnModeSet(Platform &platform, VidPn &vidpn, uint32_t owner);
    ~VidPnModeSet();
    VidPnModeSet(const VidPnModeSet &) = delete;
    VidPnModeSet &operator=(const VidPnModeSet &) = delete;

    /* The owner's current set. */
    [[nodiscard]] Status acquire();
    /* An empty set that replaces the current one once assigned. */
    [[nodiscard]] Status create();
    [[nodiscard]] Status assign();
    /* not_found when no mode is pinned. */
    [[nodiscard]] Status copy_pinned(Mode &mode);
    /* A new mode info described by `description`, keeping the Id dxgkrnl gave
     * it; a mode the set already has is not a failure. */
    [[nodiscard]] Status add(const Mode &description);

private:
    Platform               &platform_;
    VidPn                  &vidpn_;
    uint32_t                owner_;
    void                   *mode_set_;
    const ModeSetInterface *interface_;
};

using SourceModeSet = VidPnModeSet<D3dkmdtVidPnSourceModeRecord, DxgkVidPnSourceModeSetInterfaceRecord>;
using TargetModeSet = VidPnModeSet<D3dkmdtVidPnTargetModeRecord, DxgkVidPnTargetModeSetInterfaceRecord>;

template <typename Mode, typename ModeSetInterface>
VidPnModeSet<Mode, ModeSetInterface>::VidPnModeSet(Platform &platform, VidPn &vidpn, uint32_t owner)
    : platform_(platform), vidpn_(vidpn), owner_(owner), mode_set_(nullptr), interface_(nullptr) {}

/* A destructor has nowhere to report a failed release; the set is dxgkrnl's
 * to reclaim with the VidPn either way. */
template <typename Mode, typename ModeSetInterface>
VidPnModeSet<Mode, ModeSetInterface>::~VidPnModeSet()
{
    if (interface_ != nullptr) static_cast<void>(vidpn_.release_mode_set(mode_set_, interface_));
}

template <typename Mode, typename ModeSetInterface>
Status VidPnModeSet<Mode, ModeSetInterface>::acquire()
{
    platform_.require_irql_at_most(Irql::passive, "VidPnModeSet::acquire");
    if (interface_ != nullptr) return Status::invalid_state;
    return vidpn_.acquire_mode_set(owner_, mode_set_, interface_);
}

template <typename Mode, typename ModeSetInterface>
Status VidPnModeSet<Mode, ModeSetInterface>::create()
{
    platform_.require_irql_at_most(Irql::passive, "VidPnModeSet::create");
    if (interface_ != nullptr) return Status::invalid_state;
    return vidpn_.create_mode_set(owner_, mode_set_, interface_);
}

template <typename Mode, typename ModeSetInterface>
Status VidPnModeSet<Mode, ModeSetInterface>::assign()
{
    platform_.require_irql_at_most(Irql::passive, "VidPnModeSet::assign");
    if (interface_ == nullptr) return Status::invalid_state;
    const Status status = vidpn_.assign_mode_set(owner_, mode_set_, interface_);
    if (status == Status::ok) interface_ = nullptr;
    return status;
}

template <typename Mode, typename ModeSetInterface>
Status VidPnModeSet<Mode, ModeSetInterface>::copy_pinned(Mode &mode)
{
    platform_.require_irql_at_most(Irql::passive, "VidPnModeSet::copy_pinned");
    if (interface_ == nullptr) return Status::invalid_state;
    const Mode  *pinned = nullptr;
    const Status status = NtStatus(interface_->pfnAcquirePinnedModeInfo(mode_set_, &pinned)).status();
    if (status != Status::ok) return status;
    if (pinned == nullptr) return Status::not_found;
    mode = *pinned;
    return NtStatus(interface_->pfnReleaseModeInfo(mode_set_, pinned)).status();
}

/* dxgkrnl releases a mode info it adds; one it refuses is still the driver's. */
template <typename Mode, typename ModeSetInterface>
Status VidPnModeSet<Mode, ModeSetInterface>::add(const Mode &description)
{
    platform_.require_irql_at_most(Irql::passive, "VidPnModeSet::add");
    if (interface_ == nullptr) return Status::invalid_state;
    Mode        *info = nullptr;
    const Status status = NtStatus(interface_->pfnCreateNewModeInfo(mode_set_, &info)).status();
    if (status != Status::ok) return status;
    if (info == nullptr) return Status::malformed;
    const uint32_t id = info->Id;
    *info = description;
    info->Id = id;
    const NtStatus added(interface_->pfnAddMode(mode_set_, info));
    if (added.succeeded()) return Status::ok;
    const Status released = NtStatus(interface_->pfnReleaseModeInfo(mode_set_, info)).status();
    if (static_cast<uint32_t>(added.value()) == NtStatus::mode_already_in_mode_set) return released;
    return added.status();
}

}  // namespace av

#endif  // AV_VIDPN_MODE_SET_HPP
