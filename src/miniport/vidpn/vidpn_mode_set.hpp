#ifndef AV_VIDPN_MODE_SET_HPP
#define AV_VIDPN_MODE_SET_HPP

#include "miniport/ddi/dxgk_vidpn_interface_records.hpp"
#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "miniport/vidpn/vidpn.hpp"

#include <stdint.h>

namespace av {
inline void restore_created(D3dkmdtVidPnSourceModeRecord &info, const D3dkmdtVidPnSourceModeRecord &created)
{
    AV_SCOPE("restore_created");
    info.Id = created.Id;
}

inline void restore_created(D3dkmdtVidPnTargetModeRecord &info, const D3dkmdtVidPnTargetModeRecord &created)
{
    AV_SCOPE("restore_created");
    info.Id = created.Id;
    info.MinimumVSyncFreqNumerator = created.MinimumVSyncFreqNumerator;
    info.MinimumVSyncFreqDenominator = created.MinimumVSyncFreqDenominator;
}

template <typename Mode, typename ModeSetInterface>
class VidPnModeSet {
public:
    VidPnModeSet(Platform &platform, VidPn &vidpn, uint32_t owner);
    ~VidPnModeSet();
    VidPnModeSet(const VidPnModeSet &) = delete;
    VidPnModeSet &operator=(const VidPnModeSet &) = delete;

    [[nodiscard]] Status acquire();
    [[nodiscard]] Status create();
    [[nodiscard]] Status assign();
    [[nodiscard]] Status copy_pinned(Mode &mode);
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
    : platform_(platform), vidpn_(vidpn), owner_(owner), mode_set_(nullptr), interface_(nullptr)
    {
        AV_SCOPE("VidPnModeSet::VidPnModeSet");
    }

template <typename Mode, typename ModeSetInterface>
VidPnModeSet<Mode, ModeSetInterface>::~VidPnModeSet()
{
    AV_SCOPE("VidPnModeSet::~VidPnModeSet");
    if (interface_ != nullptr) static_cast<void>(vidpn_.release_mode_set(mode_set_, interface_));
}

template <typename Mode, typename ModeSetInterface>
Status VidPnModeSet<Mode, ModeSetInterface>::acquire()
{
    AV_SCOPE("VidPnModeSet::acquire");
    platform_.require_irql_at_most(Irql::passive, "VidPnModeSet::acquire");
    if (interface_ != nullptr) return Status::invalid_state;
    return vidpn_.acquire_mode_set(owner_, mode_set_, interface_);
}

template <typename Mode, typename ModeSetInterface>
Status VidPnModeSet<Mode, ModeSetInterface>::create()
{
    AV_SCOPE("VidPnModeSet::create");
    platform_.require_irql_at_most(Irql::passive, "VidPnModeSet::create");
    if (interface_ != nullptr) return Status::invalid_state;
    return vidpn_.create_mode_set(owner_, mode_set_, interface_);
}

template <typename Mode, typename ModeSetInterface>
Status VidPnModeSet<Mode, ModeSetInterface>::assign()
{
    AV_SCOPE("VidPnModeSet::assign");
    platform_.require_irql_at_most(Irql::passive, "VidPnModeSet::assign");
    if (interface_ == nullptr) return Status::invalid_state;
    const Status status = vidpn_.assign_mode_set(owner_, mode_set_, interface_);
    if (status == Status::ok) interface_ = nullptr;
    return status;
}

template <typename Mode, typename ModeSetInterface>
Status VidPnModeSet<Mode, ModeSetInterface>::copy_pinned(Mode &mode)
{
    AV_SCOPE("VidPnModeSet::copy_pinned");
    platform_.require_irql_at_most(Irql::passive, "VidPnModeSet::copy_pinned");
    if (interface_ == nullptr) return Status::invalid_state;
    const Mode  *pinned = nullptr;
    const Status status = NtStatus(interface_->pfnAcquirePinnedModeInfo(mode_set_, &pinned)).status();
    if (status != Status::ok) return status;
    if (pinned == nullptr) return Status::not_found;
    mode = *pinned;
    return NtStatus(interface_->pfnReleaseModeInfo(mode_set_, pinned)).status();
}

template <typename Mode, typename ModeSetInterface>
Status VidPnModeSet<Mode, ModeSetInterface>::add(const Mode &description)
{
    AV_SCOPE("VidPnModeSet::add");
    platform_.require_irql_at_most(Irql::passive, "VidPnModeSet::add");
    if (interface_ == nullptr) return Status::invalid_state;
    Mode        *info = nullptr;
    const Status status = NtStatus(interface_->pfnCreateNewModeInfo(mode_set_, &info)).status();
    if (status != Status::ok) return status;
    if (info == nullptr) return Status::malformed;
    const Mode created = *info;
    *info = description;
    restore_created(*info, created);
    const NtStatus added(interface_->pfnAddMode(mode_set_, info));
    if (added.succeeded()) return Status::ok;
    const Status released = NtStatus(interface_->pfnReleaseModeInfo(mode_set_, info)).status();
    if (static_cast<uint32_t>(added.value()) == NtStatus::mode_already_in_mode_set) return released;
    return added.status();
}

}

#endif
