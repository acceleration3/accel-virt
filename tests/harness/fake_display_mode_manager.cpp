#include "fake_display_mode_manager.hpp"

#include "../../src/core/ddi/dxgk_device_descriptor_record.hpp"
#include "../../src/core/ddi/dxgkarg_commit_vidpn_record.hpp"
#include "../../src/core/ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "../../src/core/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "../../src/core/ddi/dxgkarg_recommend_functional_vidpn_record.hpp"
#include "../../src/core/ddi/dxgkarg_recommend_monitor_modes_record.hpp"
#include "../../src/core/ddi/dxgkarg_recommend_vidpn_topology_record.hpp"

namespace av {

namespace {

constexpr std::int32_t no_mode_to_pin = 0x001E0310;   /* the success twin of STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE_MODE */
constexpr std::int32_t crtc_vsync = 3;                /* DXGK_INTERRUPT_CRTC_VSYNC */

using QueryDeviceDescriptor = std::int32_t (*)(void *, std::uint32_t, DxgkDeviceDescriptorRecord *);
using RecommendMonitorModes = std::int32_t (*)(void *, const DxgkargRecommendMonitorModesRecord *);
using IsSupportedVidPn = std::int32_t (*)(void *, DxgkargIsSupportedVidPnRecord *);
using EnumCofuncModality = std::int32_t (*)(void *, const DxgkargEnumVidPnCofuncModalityRecord *);
using CommitVidPn = std::int32_t (*)(void *, const DxgkargCommitVidPnRecord *);
using UpdateActivePath = std::int32_t (*)(void *, const D3dkmdtVidPnPresentPathRecord *);
using RecommendFunctional = std::int32_t (*)(void *, const DxgkargRecommendFunctionalVidPnRecord *);
using RecommendTopology = std::int32_t (*)(void *, const DxgkargRecommendVidPnTopologyRecord *);
using GetScanLine = std::int32_t (*)(void *, DxgkargGetScanLineRecord *);
using ControlInterrupt = std::int32_t (*)(void *, std::int32_t, std::uint8_t);

}  // namespace

FakeDisplayModeManager::FakeDisplayModeManager(MockPlatform &platform, const DdiTable &table, void *adapter)
    : platform_(platform), table_(table), adapter_(adapter) {}

std::int32_t FakeDisplayModeManager::read_edid(std::uint8_t *edid)
{
    platform_.set_irql(Irql::passive);
    DxgkDeviceDescriptorRecord descriptor = {0u, 128u, edid};
    return reinterpret_cast<QueryDeviceDescriptor>(table_.routine(DdiSlot::DxgkDdiQueryDeviceDescriptor))(
        adapter_, 0u, &descriptor);
}

std::int32_t FakeDisplayModeManager::read_monitor_modes(FakeVidPn &vidpn)
{
    const DxgkargRecommendMonitorModesRecord request = {0u, 0u, vidpn.monitor_mode_set(), &vidpn.monitor_interface()};
    return reinterpret_cast<RecommendMonitorModes>(table_.routine(DdiSlot::DxgkDdiRecommendMonitorModes))(adapter_,
                                                                                                           &request);
}

std::int32_t FakeDisplayModeManager::set_mode(FakeVidPn &vidpn, std::uint32_t width, std::uint32_t height)
{
    if (vidpn.paths() == 0u) vidpn.add_path(0u, 0u);
    bool         supported = false;
    std::int32_t status = is_supported(vidpn, supported);
    if (status == 0) status = enumerate(vidpn, no_pivot);
    if (status != 0) return status;
    const FakeModeSet<D3dkmdtVidPnSourceModeRecord> &sources = vidpn.source_modes(0u);
    std::uint32_t source = sources.modes();
    for (std::uint32_t index = 0u; index < sources.modes(); index++)
        if (sources.mode(index).PrimSurfSizeCx == width && sources.mode(index).PrimSurfSizeCy == height) source = index;
    if (source == sources.modes()) return no_mode_to_pin;
    status = vidpn.source_modes(0u).pin(sources.mode(source).Id);
    if (status == 0) status = enumerate(vidpn, pivot_source);
    if (status != 0) return status;
    const FakeModeSet<D3dkmdtVidPnTargetModeRecord> &targets = vidpn.target_modes(0u);
    if (targets.modes() != 1u) return no_mode_to_pin;
    status = vidpn.target_modes(0u).pin(targets.mode(0u).Id);
    if (status == 0) status = is_supported(vidpn, supported);
    if (status != 0 || !supported) return status;
    return commit(vidpn, 0u);
}

std::int32_t FakeDisplayModeManager::is_supported(FakeVidPn &vidpn, bool &supported)
{
    platform_.set_irql(Irql::passive);
    DxgkargIsSupportedVidPnRecord request = {&vidpn, 0u, {}};
    const std::int32_t status =
        reinterpret_cast<IsSupportedVidPn>(table_.routine(DdiSlot::DxgkDdiIsSupportedVidPn))(adapter_, &request);
    supported = request.IsVidPnSupported != 0u;
    return status;
}

std::int32_t FakeDisplayModeManager::commit(FakeVidPn &vidpn, std::uint32_t flags)
{
    const DxgkargCommitVidPnRecord request = {&vidpn, 0u, 0, nullptr, flags, 0u};
    return reinterpret_cast<CommitVidPn>(table_.routine(DdiSlot::DxgkDdiCommitVidPn))(adapter_, &request);
}

std::int32_t FakeDisplayModeManager::update_active_path(const D3dkmdtVidPnPresentPathRecord &path)
{
    return reinterpret_cast<UpdateActivePath>(table_.routine(DdiSlot::DxgkDdiUpdateActiveVidPnPresentPath))(adapter_,
                                                                                                             &path);
}

std::int32_t FakeDisplayModeManager::recommend_functional_vidpn()
{
    const DxgkargRecommendFunctionalVidPnRecord request = {};
    return reinterpret_cast<RecommendFunctional>(table_.routine(DdiSlot::DxgkDdiRecommendFunctionalVidPn))(adapter_,
                                                                                                            &request);
}

std::int32_t FakeDisplayModeManager::recommend_vidpn_topology()
{
    const DxgkargRecommendVidPnTopologyRecord request = {};
    return reinterpret_cast<RecommendTopology>(table_.routine(DdiSlot::DxgkDdiRecommendVidPnTopology))(adapter_,
                                                                                                        &request);
}

std::int32_t FakeDisplayModeManager::scan_line(DxgkargGetScanLineRecord &scan)
{
    return reinterpret_cast<GetScanLine>(table_.routine(DdiSlot::DxgkDdiGetScanLine))(adapter_, &scan);
}

std::int32_t FakeDisplayModeManager::control_vsync(bool enable)
{
    return reinterpret_cast<ControlInterrupt>(table_.routine(DdiSlot::DxgkDdiControlInterrupt))(adapter_, crtc_vsync,
                                                                                                 enable ? 1u : 0u);
}

std::int32_t FakeDisplayModeManager::enumerate(FakeVidPn &vidpn, std::int32_t pivot)
{
    const DxgkargEnumVidPnCofuncModalityRecord request = {&vidpn, pivot, 0u, 0u, 0u};
    return reinterpret_cast<EnumCofuncModality>(table_.routine(DdiSlot::DxgkDdiEnumVidPnCofuncModality))(adapter_,
                                                                                                          &request);
}

}  // namespace av
