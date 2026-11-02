#include "harness/vidpn/fake_vidpn.hpp"

#include "harness/framework/test_registry.hpp"

namespace av {

namespace {

constexpr std::int32_t source_not_in_topology = static_cast<std::int32_t>(0xC01E0339u);
constexpr std::int32_t invalid_parameter = static_cast<std::int32_t>(0xC000000Du);
constexpr std::int32_t unpinned_transformation = 254;
constexpr std::uint32_t offset_zero = 0x10u;
constexpr std::uint32_t other_offsets = 0xE0u;
constexpr std::int32_t empty_source_mode_set = static_cast<std::int32_t>(0xC01E0308u);
constexpr std::int32_t empty_target_mode_set = static_cast<std::int32_t>(0xC01E0309u);
constexpr std::int32_t no_more_elements = 0x401E034C;
constexpr std::int32_t gamma_default = 1;

FakeVidPn *owner_of(void *topology) { return *static_cast<FakeVidPn **>(topology); }

}

FakeVidPn::FakeVidPn()
    : sources_{}, targets_{}, current_source_{}, current_target_{}, source_handles_{}, target_handles_{}, monitor_{},
      monitor_handle_{this, &monitor_}, refused_{}, refusals_{}, paths_{}, path_count_(0u), path_updates_(0u),
      outstanding_paths_(0u), outstanding_sets_(0u), topology_handle_(this), vidpn_interface_{},
      topology_interface_{}, source_interface_{}, target_interface_{}, monitor_interface_{}
{
    for (std::uint32_t owner = 0u; owner < max_owners; owner++) {
        for (std::uint32_t slot = 0u; slot < 2u; slot++) {
            source_handles_[owner][slot] = {this, &sources_[owner][slot]};
            target_handles_[owner][slot] = {this, &targets_[owner][slot]};
        }
    }
    install_vidpn_interface();
    install_topology_interface();
    install_source_interface();
    install_target_interface();
    install_monitor_interface();
}

FakeVidPn::~FakeVidPn()
{
    if (outstanding_paths_ != 0u || outstanding_sets_ != 0u)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "%u path info(s), %u mode set(s) never released",
                                                outstanding_paths_, outstanding_sets_);
}

void FakeVidPn::install_vidpn_interface()
{
    vidpn_interface_.Version = 1;
    vidpn_interface_.pfnGetTopology = +[](void *vidpn, void **topology, const DxgkVidPnTopologyInterfaceRecord **table) {
        auto *self = static_cast<FakeVidPn *>(vidpn);
        std::int32_t status = 0;
        const bool refused = self->refusing(VidPnCall::get_topology, status);
        *topology = refused ? nullptr : &self->topology_handle_;
        *table = refused ? nullptr : &self->topology_interface_;
        return status;
    };
    vidpn_interface_.pfnAcquireSourceModeSet = +[](void *vidpn, std::uint32_t source, void **set,
                                                   const DxgkVidPnSourceModeSetInterfaceRecord **table) {
        auto *self = static_cast<FakeVidPn *>(vidpn);
        std::int32_t status = 0;
        *set = nullptr;
        *table = nullptr;
        if (self->refusing(VidPnCall::acquire_source_mode_set, status)) return status;
        *table = &self->source_interface_;
        return self->acquire_source(source, false, set);
    };
    vidpn_interface_.pfnCreateNewSourceModeSet = +[](void *vidpn, std::uint32_t source, void **set,
                                                     const DxgkVidPnSourceModeSetInterfaceRecord **table) {
        auto *self = static_cast<FakeVidPn *>(vidpn);
        std::int32_t status = 0;
        *set = nullptr;
        *table = nullptr;
        if (self->refusing(VidPnCall::create_source_mode_set, status)) return status;
        *table = &self->source_interface_;
        return self->acquire_source(source, true, set);
    };
    vidpn_interface_.pfnReleaseSourceModeSet = +[](void *vidpn, void *set) {
        return static_cast<FakeVidPn *>(vidpn)->release_set(set);
    };
    vidpn_interface_.pfnAssignSourceModeSet = +[](void *vidpn, std::uint32_t source, void *set) {
        auto *self = static_cast<FakeVidPn *>(vidpn);
        std::int32_t status = 0;
        if (self->refusing(VidPnCall::assign_source_mode_set, status)) return status < 0 ? status : self->release_set(set);
        return self->assign_source(source, set);
    };
    vidpn_interface_.pfnAcquireTargetModeSet = +[](void *vidpn, std::uint32_t target, void **set,
                                                   const DxgkVidPnTargetModeSetInterfaceRecord **table) {
        auto *self = static_cast<FakeVidPn *>(vidpn);
        std::int32_t status = 0;
        *set = nullptr;
        *table = nullptr;
        if (self->refusing(VidPnCall::acquire_target_mode_set, status)) return status;
        *table = &self->target_interface_;
        return self->acquire_target(target, false, set);
    };
    vidpn_interface_.pfnCreateNewTargetModeSet = +[](void *vidpn, std::uint32_t target, void **set,
                                                     const DxgkVidPnTargetModeSetInterfaceRecord **table) {
        auto *self = static_cast<FakeVidPn *>(vidpn);
        std::int32_t status = 0;
        *set = nullptr;
        *table = nullptr;
        if (self->refusing(VidPnCall::create_target_mode_set, status)) return status;
        *table = &self->target_interface_;
        return self->acquire_target(target, true, set);
    };
    vidpn_interface_.pfnReleaseTargetModeSet = +[](void *vidpn, void *set) {
        return static_cast<FakeVidPn *>(vidpn)->release_set(set);
    };
    vidpn_interface_.pfnAssignTargetModeSet = +[](void *vidpn, std::uint32_t target, void *set) {
        auto *self = static_cast<FakeVidPn *>(vidpn);
        std::int32_t status = 0;
        if (self->refusing(VidPnCall::assign_target_mode_set, status)) return status < 0 ? status : self->release_set(set);
        return self->assign_target(target, set);
    };
}

void FakeVidPn::install_topology_interface()
{
    topology_interface_.pfnGetNumPaths = +[](void *topology, std::uint64_t *paths) {
        *paths = owner_of(topology)->path_count_;
        return 0;
    };
    topology_interface_.pfnGetNumPathsFromSource = +[](void *topology, std::uint32_t source, std::uint64_t *paths) {
        const FakeVidPn *self = owner_of(topology);
        std::int32_t status = 0;
        *paths = 0u;
        if (self->refusing(VidPnCall::paths_from_source, status)) return status;
        for (std::uint32_t index = 0u; index < self->path_count_; index++)
            *paths += self->paths_[index].VidPnSourceId == source ? 1u : 0u;
        return *paths == 0u ? source_not_in_topology : 0;
    };
    topology_interface_.pfnEnumPathTargetsFromSource = +[](void *topology, std::uint32_t source, std::uint64_t index,
                                                           std::uint32_t *target) {
        const FakeVidPn *self = owner_of(topology);
        std::uint64_t seen = 0u;
        for (std::uint32_t path = 0u; path < self->path_count_; path++) {
            if (self->paths_[path].VidPnSourceId != source) continue;
            if (seen++ != index) continue;
            *target = self->paths_[path].VidPnTargetId;
            return 0;
        }
        return source_not_in_topology;
    };
    topology_interface_.pfnAcquireFirstPathInfo = +[](void *topology, const D3dkmdtVidPnPresentPathRecord **path) {
        FakeVidPn *self = owner_of(topology);
        std::int32_t status = 0;
        *path = nullptr;
        if (self->refusing(VidPnCall::acquire_first_path, status)) return status;
        return self->acquire_path(0u, path);
    };
    topology_interface_.pfnAcquireNextPathInfo = +[](void *topology, const D3dkmdtVidPnPresentPathRecord *path,
                                                     const D3dkmdtVidPnPresentPathRecord **next) {
        FakeVidPn *self = owner_of(topology);
        std::int32_t status = 0;
        *next = nullptr;
        if (self->refusing(VidPnCall::acquire_next_path, status)) return status;
        return self->acquire_path(self->index_of(path) + 1u, next);
    };
    topology_interface_.pfnAcquirePathInfo = +[](void *topology, std::uint32_t source, std::uint32_t target,
                                                 const D3dkmdtVidPnPresentPathRecord **path) {
        FakeVidPn *self = owner_of(topology);
        for (std::uint32_t index = 0u; index < self->path_count_; index++)
            if (self->paths_[index].VidPnSourceId == source && self->paths_[index].VidPnTargetId == target)
                return self->acquire_path(index, path);
        *path = nullptr;
        return invalid_parameter;
    };
    topology_interface_.pfnUpdatePathSupportInfo = +[](void *topology, const D3dkmdtVidPnPresentPathRecord *path) {
        FakeVidPn *self = owner_of(topology);
        std::int32_t status = 0;
        if (self->refusing(VidPnCall::update_path_support, status)) return status;
        return self->update_path(*path);
    };
    topology_interface_.pfnReleasePathInfo = +[](void *topology, const D3dkmdtVidPnPresentPathRecord *) {
        FakeVidPn *self = owner_of(topology);
        if (self->outstanding_paths_ == 0u) {
            TestRegistry::instance().record_failure(__FILE__, __LINE__, "released a path info that was not held");
            return invalid_parameter;
        }
        self->outstanding_paths_--;
        std::int32_t status = 0;
        static_cast<void>(self->refusing(VidPnCall::release_path, status));
        return status;
    };
}

void FakeVidPn::install_source_interface()
{
    source_interface_.pfnGetNumModes = +[](void *set, std::uint64_t *modes) {
        return static_cast<SourceHandle *>(set)->set->count(modes);
    };
    source_interface_.pfnAcquireFirstModeInfo = +[](void *set, const D3dkmdtVidPnSourceModeRecord **mode) {
        return static_cast<SourceHandle *>(set)->set->acquire_first(mode);
    };
    source_interface_.pfnAcquireNextModeInfo = +[](void *set, const D3dkmdtVidPnSourceModeRecord *mode,
                                                   const D3dkmdtVidPnSourceModeRecord **next) {
        return static_cast<SourceHandle *>(set)->set->acquire_next(mode, next);
    };
    source_interface_.pfnAcquirePinnedModeInfo = +[](void *set, const D3dkmdtVidPnSourceModeRecord **mode) {
        const SourceHandle *handle = static_cast<SourceHandle *>(set);
        std::int32_t status = 0;
        *mode = nullptr;
        if (handle->vidpn->refusing(VidPnCall::acquire_pinned_mode, status)) return status;
        return handle->set->acquire_pinned(mode);
    };
    source_interface_.pfnReleaseModeInfo = +[](void *set, const D3dkmdtVidPnSourceModeRecord *mode) {
        return static_cast<SourceHandle *>(set)->set->release(mode);
    };
    source_interface_.pfnCreateNewModeInfo = +[](void *set, D3dkmdtVidPnSourceModeRecord **mode) {
        const SourceHandle *handle = static_cast<SourceHandle *>(set);
        std::int32_t status = 0;
        *mode = nullptr;
        if (handle->vidpn->refusing(VidPnCall::create_mode_info, status)) return status;
        return handle->set->create_new(mode);
    };
    source_interface_.pfnAddMode = +[](void *set, const D3dkmdtVidPnSourceModeRecord *mode) {
        const SourceHandle *handle = static_cast<SourceHandle *>(set);
        std::int32_t status = 0;
        if (handle->vidpn->refusing(VidPnCall::add_mode, status)) return status < 0 ? status : handle->set->release(mode);
        return handle->set->add(mode);
    };
    source_interface_.pfnPinMode = +[](void *set, std::uint32_t id) {
        return static_cast<SourceHandle *>(set)->set->pin(id);
    };
}

void FakeVidPn::install_target_interface()
{
    target_interface_.pfnGetNumModes = +[](void *set, std::uint64_t *modes) {
        return static_cast<TargetHandle *>(set)->set->count(modes);
    };
    target_interface_.pfnAcquireFirstModeInfo = +[](void *set, const D3dkmdtVidPnTargetModeRecord **mode) {
        return static_cast<TargetHandle *>(set)->set->acquire_first(mode);
    };
    target_interface_.pfnAcquireNextModeInfo = +[](void *set, const D3dkmdtVidPnTargetModeRecord *mode,
                                                   const D3dkmdtVidPnTargetModeRecord **next) {
        return static_cast<TargetHandle *>(set)->set->acquire_next(mode, next);
    };
    target_interface_.pfnAcquirePinnedModeInfo = +[](void *set, const D3dkmdtVidPnTargetModeRecord **mode) {
        const TargetHandle *handle = static_cast<TargetHandle *>(set);
        std::int32_t status = 0;
        *mode = nullptr;
        if (handle->vidpn->refusing(VidPnCall::acquire_pinned_mode, status)) return status;
        return handle->set->acquire_pinned(mode);
    };
    target_interface_.pfnReleaseModeInfo = +[](void *set, const D3dkmdtVidPnTargetModeRecord *mode) {
        return static_cast<TargetHandle *>(set)->set->release(mode);
    };
    target_interface_.pfnCreateNewModeInfo = +[](void *set, D3dkmdtVidPnTargetModeRecord **mode) {
        const TargetHandle *handle = static_cast<TargetHandle *>(set);
        std::int32_t status = 0;
        *mode = nullptr;
        if (handle->vidpn->refusing(VidPnCall::create_mode_info, status)) return status;
        return handle->set->create_new(mode);
    };
    target_interface_.pfnAddMode = +[](void *set, const D3dkmdtVidPnTargetModeRecord *mode) {
        const TargetHandle *handle = static_cast<TargetHandle *>(set);
        std::int32_t status = 0;
        if (handle->vidpn->refusing(VidPnCall::add_mode, status)) return status < 0 ? status : handle->set->release(mode);
        return handle->set->add(mode);
    };
    target_interface_.pfnPinMode = +[](void *set, std::uint32_t id) {
        return static_cast<TargetHandle *>(set)->set->pin(id);
    };
}

void FakeVidPn::install_monitor_interface()
{
    monitor_interface_.pfnGetNumModes = +[](void *set, std::uint64_t *modes) {
        return static_cast<MonitorHandle *>(set)->set->count(modes);
    };
    monitor_interface_.pfnCreateNewModeInfo = +[](void *set, D3dkmdtMonitorSourceModeRecord **mode) {
        const MonitorHandle *handle = static_cast<MonitorHandle *>(set);
        std::int32_t status = 0;
        *mode = nullptr;
        if (handle->vidpn->refusing(VidPnCall::create_mode_info, status)) return status;
        return handle->set->create_new(mode);
    };
    monitor_interface_.pfnAddMode = +[](void *set, const D3dkmdtMonitorSourceModeRecord *mode) {
        const MonitorHandle *handle = static_cast<MonitorHandle *>(set);
        std::int32_t status = 0;
        if (handle->vidpn->refusing(VidPnCall::add_mode, status)) return status < 0 ? status : handle->set->release(mode);
        return handle->set->add(mode);
    };
    monitor_interface_.pfnReleaseModeInfo = +[](void *set, const D3dkmdtMonitorSourceModeRecord *mode) {
        return static_cast<MonitorHandle *>(set)->set->release(mode);
    };
}

D3dkmdtVidPnPresentPathRecord &FakeVidPn::add_path(std::uint32_t source, std::uint32_t target)
{
    D3dkmdtVidPnPresentPathRecord &path = paths_[path_count_ < max_paths ? path_count_++ : max_paths - 1u];
    path = D3dkmdtVidPnPresentPathRecord{};
    path.VidPnSourceId = source;
    path.VidPnTargetId = target;
    path.ImportanceOrdinal = 1;
    path.GammaRampType = gamma_default;
    path.ContentTransformationScaling = unpinned_transformation;
    path.ContentTransformationRotation = unpinned_transformation;
    return path;
}

FakeModeSet<D3dkmdtVidPnSourceModeRecord> &FakeVidPn::source_modes(std::uint32_t source)
{
    const std::uint32_t owner = source < max_owners ? source : 0u;
    return sources_[owner][current_source_[owner]];
}

FakeModeSet<D3dkmdtVidPnTargetModeRecord> &FakeVidPn::target_modes(std::uint32_t target)
{
    const std::uint32_t owner = target < max_owners ? target : 0u;
    return targets_[owner][current_target_[owner]];
}

const D3dkmdtVidPnPresentPathRecord &FakeVidPn::path(std::uint32_t index) const
{
    return paths_[index < max_paths ? index : 0u];
}

std::uint32_t FakeVidPn::paths() const { return path_count_; }
std::uint32_t FakeVidPn::path_support_updates() const { return path_updates_; }

std::int32_t FakeVidPn::query(std::int32_t version, const DxgkVidPnInterfaceRecord **table) const
{
    *table = version == 1 ? &vidpn_interface_ : nullptr;
    return version == 1 ? 0 : invalid_parameter;
}

void FakeVidPn::refuse(VidPnCall call, std::int32_t status)
{
    refused_[static_cast<unsigned>(call)] = true;
    refusals_[static_cast<unsigned>(call)] = status;
}

bool FakeVidPn::refusing(VidPnCall call, std::int32_t &status) const
{
    status = refusals_[static_cast<unsigned>(call)];
    return refused_[static_cast<unsigned>(call)];
}

const DxgkMonitorSourceModeSetInterfaceRecord &FakeVidPn::monitor_interface() const { return monitor_interface_; }
FakeModeSet<D3dkmdtMonitorSourceModeRecord>   &FakeVidPn::monitor_modes() { return monitor_; }
void                                          *FakeVidPn::monitor_mode_set() { return &monitor_handle_; }

std::int32_t FakeVidPn::acquire_source(std::uint32_t source, bool fresh, void **mode_set)
{
    if (source >= max_owners) return invalid_parameter;
    std::uint32_t slot = current_source_[source];
    if (fresh) {
        slot = 1u - slot;
        sources_[source][slot].clear();
    }
    *mode_set = &source_handles_[source][slot];
    outstanding_sets_++;
    return 0;
}

std::int32_t FakeVidPn::acquire_target(std::uint32_t target, bool fresh, void **mode_set)
{
    if (target >= max_owners) return invalid_parameter;
    std::uint32_t slot = current_target_[target];
    if (fresh) {
        slot = 1u - slot;
        targets_[target][slot].clear();
    }
    *mode_set = &target_handles_[target][slot];
    outstanding_sets_++;
    return 0;
}

std::int32_t FakeVidPn::release_set(void *)
{
    if (outstanding_sets_ == 0u) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "released a mode set that was not held");
        return invalid_parameter;
    }
    outstanding_sets_--;
    return 0;
}

std::int32_t FakeVidPn::assign_source(std::uint32_t source, void *mode_set)
{
    if (source >= max_owners) return invalid_parameter;
    for (std::uint32_t slot = 0u; slot < 2u; slot++)
        if (mode_set == &source_handles_[source][slot]) {
            if (sources_[source][slot].modes() == 0u) return empty_source_mode_set;
            current_source_[source] = slot;
        }
    return release_set(mode_set);
}

std::int32_t FakeVidPn::assign_target(std::uint32_t target, void *mode_set)
{
    if (target >= max_owners) return invalid_parameter;
    for (std::uint32_t slot = 0u; slot < 2u; slot++)
        if (mode_set == &target_handles_[target][slot]) {
            if (targets_[target][slot].modes() == 0u) return empty_target_mode_set;
            current_target_[target] = slot;
        }
    return release_set(mode_set);
}

std::int32_t FakeVidPn::acquire_path(std::uint32_t index, const D3dkmdtVidPnPresentPathRecord **path)
{
    *path = index < path_count_ ? &paths_[index] : nullptr;
    if (*path == nullptr) return no_more_elements;
    outstanding_paths_++;
    return 0;
}

std::int32_t FakeVidPn::update_path(const D3dkmdtVidPnPresentPathRecord &path)
{
    const std::uint32_t rotation = path.ContentTransformationRotationSupport;
    if ((rotation & offset_zero) == 0u || (rotation & other_offsets) != 0u)
        TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                "a rotation support mask dxgkrnl would bugcheck on");
    for (std::uint32_t index = 0u; index < path_count_; index++) {
        if (paths_[index].VidPnSourceId != path.VidPnSourceId || paths_[index].VidPnTargetId != path.VidPnTargetId)
            continue;
        paths_[index].ContentTransformationScalingSupport = path.ContentTransformationScalingSupport;
        paths_[index].ContentTransformationRotationSupport = path.ContentTransformationRotationSupport;
        path_updates_++;
        return 0;
    }
    return invalid_parameter;
}

std::uint32_t FakeVidPn::index_of(const D3dkmdtVidPnPresentPathRecord *path) const
{
    for (std::uint32_t index = 0u; index < path_count_; index++)
        if (&paths_[index] == path) return index;
    return max_paths;
}

}

extern "C" std::int32_t AvFakeQueryVidPnInterface(void *vidpn, std::int32_t version,
                                                  const av::DxgkVidPnInterfaceRecord **table)
{ return static_cast<const av::FakeVidPn *>(vidpn)->query(version, table); }
