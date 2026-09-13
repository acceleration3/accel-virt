#include "core/vidpn/vidpn.hpp"

#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/support/nt_status.hpp"
#include "core/vidpn/path_visitor.hpp"

namespace av {

VidPn::VidPn(Platform &platform, DxgkrnlInterface &dxgkrnl, void *handle)
    : platform_(platform), dxgkrnl_(dxgkrnl), handle_(handle), interface_(nullptr), topology_(nullptr),
      topology_interface_(nullptr) {}

Status VidPn::initialize()
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::initialize");
    if (handle_ == nullptr) return Status::invalid_parameter;
    Status status = dxgkrnl_.query_vidpn_interface(handle_, interface_);
    if (status != Status::ok) return status;
    status = NtStatus(interface_->pfnGetTopology(handle_, &topology_, &topology_interface_)).status();
    if (status == Status::ok && topology_interface_ == nullptr) return Status::malformed;
    return status;
}

Status VidPn::source_paths(uint32_t source, uint64_t &paths)
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::source_paths");
    paths = 0u;
    if (topology_interface_ == nullptr) return Status::invalid_state;
    const int32_t answer = topology_interface_->pfnGetNumPathsFromSource(topology_, source, &paths);
    if (static_cast<uint32_t>(answer) == NtStatus::source_not_in_topology) {
        paths = 0u;
        return Status::ok;
    }
    return NtStatus(answer).status();
}

/* A path is released only after the next is acquired from it. Once anything
 * fails no further path is visited, but the one already acquired is still
 * released. */
Status VidPn::for_each_path(PathVisitor &visitor)
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::for_each_path");
    if (topology_interface_ == nullptr) return Status::invalid_state;
    const D3dkmdtVidPnPresentPathRecord *path = nullptr;
    int32_t answer = topology_interface_->pfnAcquireFirstPathInfo(topology_, &path);
    Status  status = Status::ok;
    while (answer >= 0 && path != nullptr) {
        const D3dkmdtVidPnPresentPathRecord *next = nullptr;
        if (status == Status::ok) status = visitor.visit(*path);
        if (status == Status::ok) answer = topology_interface_->pfnAcquireNextPathInfo(topology_, path, &next);
        const Status released = release_path(path);
        if (status == Status::ok) status = released;
        path = next;
    }
    return status != Status::ok ? status : NtStatus(answer).status();
}

Status VidPn::update_path_support(const D3dkmdtVidPnPresentPathRecord &path)
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::update_path_support");
    if (topology_interface_ == nullptr) return Status::invalid_state;
    return NtStatus(topology_interface_->pfnUpdatePathSupportInfo(topology_, &path)).status();
}

Status VidPn::acquire_mode_set(uint32_t source, void *&mode_set,
                               const DxgkVidPnSourceModeSetInterfaceRecord *&mode_set_interface)
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::acquire_mode_set");
    if (topology_interface_ == nullptr) return Status::invalid_state;
    return obtain(interface_->pfnAcquireSourceModeSet, source, mode_set, mode_set_interface);
}

Status VidPn::acquire_mode_set(uint32_t target, void *&mode_set,
                               const DxgkVidPnTargetModeSetInterfaceRecord *&mode_set_interface)
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::acquire_mode_set");
    if (topology_interface_ == nullptr) return Status::invalid_state;
    return obtain(interface_->pfnAcquireTargetModeSet, target, mode_set, mode_set_interface);
}

Status VidPn::create_mode_set(uint32_t source, void *&mode_set,
                              const DxgkVidPnSourceModeSetInterfaceRecord *&mode_set_interface)
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::create_mode_set");
    if (topology_interface_ == nullptr) return Status::invalid_state;
    return obtain(interface_->pfnCreateNewSourceModeSet, source, mode_set, mode_set_interface);
}

Status VidPn::create_mode_set(uint32_t target, void *&mode_set,
                              const DxgkVidPnTargetModeSetInterfaceRecord *&mode_set_interface)
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::create_mode_set");
    if (topology_interface_ == nullptr) return Status::invalid_state;
    return obtain(interface_->pfnCreateNewTargetModeSet, target, mode_set, mode_set_interface);
}

Status VidPn::release_mode_set(void *mode_set, const DxgkVidPnSourceModeSetInterfaceRecord *)
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::release_mode_set");
    if (topology_interface_ == nullptr) return Status::invalid_state;
    return NtStatus(interface_->pfnReleaseSourceModeSet(handle_, mode_set)).status();
}

Status VidPn::release_mode_set(void *mode_set, const DxgkVidPnTargetModeSetInterfaceRecord *)
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::release_mode_set");
    if (topology_interface_ == nullptr) return Status::invalid_state;
    return NtStatus(interface_->pfnReleaseTargetModeSet(handle_, mode_set)).status();
}

Status VidPn::assign_mode_set(uint32_t source, void *mode_set, const DxgkVidPnSourceModeSetInterfaceRecord *)
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::assign_mode_set");
    if (topology_interface_ == nullptr) return Status::invalid_state;
    return NtStatus(interface_->pfnAssignSourceModeSet(handle_, source, mode_set)).status();
}

Status VidPn::assign_mode_set(uint32_t target, void *mode_set, const DxgkVidPnTargetModeSetInterfaceRecord *)
{
    platform_.require_irql_at_most(Irql::passive, "VidPn::assign_mode_set");
    if (topology_interface_ == nullptr) return Status::invalid_state;
    return NtStatus(interface_->pfnAssignTargetModeSet(handle_, target, mode_set)).status();
}

template <typename ModeSetInterface>
Status VidPn::obtain(int32_t (*call)(void *, uint32_t, void **, const ModeSetInterface **), uint32_t owner,
                     void *&mode_set, const ModeSetInterface *&mode_set_interface)
{
    mode_set = nullptr;
    mode_set_interface = nullptr;
    const Status status = NtStatus(call(handle_, owner, &mode_set, &mode_set_interface)).status();
    if (status == Status::ok && mode_set_interface != nullptr && mode_set != nullptr) return Status::ok;
    /* A call that failed holds nothing, whatever it left in the out parameters:
     * keeping its interface makes the caller release a set it never acquired. */
    mode_set = nullptr;
    mode_set_interface = nullptr;
    return status == Status::ok ? Status::malformed : status;
}

Status VidPn::release_path(const D3dkmdtVidPnPresentPathRecord *path)
{
    return NtStatus(topology_interface_->pfnReleasePathInfo(topology_, path)).status();
}

}  // namespace av
