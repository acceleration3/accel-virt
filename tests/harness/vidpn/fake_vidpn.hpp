#ifndef AV_FAKE_VIDPN_HPP
#define AV_FAKE_VIDPN_HPP

#include "miniport/ddi/dxgk_vidpn_interface_records.hpp"
#include "harness/vidpn/fake_mode_set.hpp"
#include "harness/vidpn/vidpn_call.hpp"

#include <cstdint>

namespace av {

class FakeVidPn {
public:
    static constexpr std::uint32_t max_paths = 4u;
    static constexpr std::uint32_t max_owners = 2u;

    FakeVidPn();
    ~FakeVidPn();
    FakeVidPn(const FakeVidPn &) = delete;
    FakeVidPn &operator=(const FakeVidPn &) = delete;

    D3dkmdtVidPnPresentPathRecord &add_path(std::uint32_t source, std::uint32_t target);
    FakeModeSet<D3dkmdtVidPnSourceModeRecord> &source_modes(std::uint32_t source);
    FakeModeSet<D3dkmdtVidPnTargetModeRecord> &target_modes(std::uint32_t target);
    const D3dkmdtVidPnPresentPathRecord       &path(std::uint32_t index) const;
    std::uint32_t                              paths() const;
    std::uint32_t                              path_support_updates() const;

    std::int32_t query(std::int32_t version, const DxgkVidPnInterfaceRecord **table) const;
    void         refuse(VidPnCall call, std::int32_t status);

    const DxgkMonitorSourceModeSetInterfaceRecord &monitor_interface() const;
    FakeModeSet<D3dkmdtMonitorSourceModeRecord>   &monitor_modes();
    void                                          *monitor_mode_set();

private:
    struct SourceHandle {
        FakeVidPn                                 *vidpn;
        FakeModeSet<D3dkmdtVidPnSourceModeRecord> *set;
    };
    struct TargetHandle {
        FakeVidPn                                 *vidpn;
        FakeModeSet<D3dkmdtVidPnTargetModeRecord> *set;
    };
    struct MonitorHandle {
        FakeVidPn                                   *vidpn;
        FakeModeSet<D3dkmdtMonitorSourceModeRecord> *set;
    };

    bool refusing(VidPnCall call, std::int32_t &status) const;
    void install_vidpn_interface();
    void install_topology_interface();
    void install_source_interface();
    void install_target_interface();
    void install_monitor_interface();

    std::int32_t acquire_source(std::uint32_t source, bool fresh, void **mode_set);
    std::int32_t acquire_target(std::uint32_t target, bool fresh, void **mode_set);
    std::int32_t release_set(void *mode_set);
    std::int32_t assign_source(std::uint32_t source, void *mode_set);
    std::int32_t assign_target(std::uint32_t target, void *mode_set);
    std::int32_t acquire_path(std::uint32_t index, const D3dkmdtVidPnPresentPathRecord **path);
    std::int32_t update_path(const D3dkmdtVidPnPresentPathRecord &path);
    std::uint32_t index_of(const D3dkmdtVidPnPresentPathRecord *path) const;

    FakeModeSet<D3dkmdtVidPnSourceModeRecord> sources_[max_owners][2];
    FakeModeSet<D3dkmdtVidPnTargetModeRecord> targets_[max_owners][2];
    std::uint32_t                             current_source_[max_owners];
    std::uint32_t                             current_target_[max_owners];
    SourceHandle                              source_handles_[max_owners][2];
    TargetHandle                              target_handles_[max_owners][2];
    FakeModeSet<D3dkmdtMonitorSourceModeRecord> monitor_;
    MonitorHandle                             monitor_handle_;
    bool                                      refused_[static_cast<unsigned>(VidPnCall::count)];
    std::int32_t                              refusals_[static_cast<unsigned>(VidPnCall::count)];
    D3dkmdtVidPnPresentPathRecord             paths_[max_paths];
    std::uint32_t                             path_count_;
    std::uint32_t                             path_updates_;
    std::uint32_t                             outstanding_paths_;
    std::uint32_t                             outstanding_sets_;
    FakeVidPn                                *topology_handle_;
    DxgkVidPnInterfaceRecord                  vidpn_interface_;
    DxgkVidPnTopologyInterfaceRecord          topology_interface_;
    DxgkVidPnSourceModeSetInterfaceRecord     source_interface_;
    DxgkVidPnTargetModeSetInterfaceRecord     target_interface_;
    DxgkMonitorSourceModeSetInterfaceRecord   monitor_interface_;
};

}

extern "C" std::int32_t AvFakeQueryVidPnInterface(void *vidpn, std::int32_t version,
                                                  const av::DxgkVidPnInterfaceRecord **table);

#endif
