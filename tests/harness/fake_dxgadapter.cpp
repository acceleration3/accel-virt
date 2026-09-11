#include "fake_dxgadapter.hpp"

#include "../../src/core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "test_registry.hpp"

#include <cstring>

namespace av {

namespace {

using QueryAdapterInfo = std::int32_t (*)(void *, const DxgkargQueryAdapterInfoRecord *);

constexpr std::int32_t status_not_implemented = static_cast<std::int32_t>(0xC0000002u);
constexpr std::int32_t status_no_memory = static_cast<std::int32_t>(0xC0000017u);

/* DXGK_QUERYADAPTERINFOTYPE */
enum : std::int32_t {
    query_driver_caps = 1,
    query_physical_adapter_caps = 15,
    query_frame_buffer_save_size = 21,
    query_native_fence_caps = 37,
    query_dirty_bit_tracking_caps = 39,
    query_64_bit_only_caps = 47,
};

/* DXGK_WDDMVERSION */
enum : std::int32_t {
    wddm_1_2 = 0x1200, wddm_1_3 = 0x1300, wddm_2_0 = 0x2000, wddm_2_2 = 0x2200,
    wddm_2_3 = 0x2300, wddm_2_4 = 0x2400, wddm_2_5 = 0x2500, wddm_3_2 = 0x3200,
};
constexpr std::int32_t known_versions[] = {0x1000, 0x1200, 0x1300, 0x2000, 0x2100, 0x2200, 0x2300, 0x2400,
                                           0x2500, 0x2600, 0x2700, 0x2800, 0x2900, 0x3000, 0x3100, 0x3200};

/* Bits of the DXGK_DRIVERCAPS unions DXGADAPTER::Initialize tests. */
constexpr std::uint32_t scheduling_multi_engine_aware = 0x1u;
constexpr std::uint32_t scheduling_wddm_1_2_bits = 0x14u;   /* PreemptionAware | CancelCommandAware */
constexpr std::uint32_t scheduling_native_gpu_fence = 0x800u;
constexpr std::uint32_t memory_cross_adapter_resource = 0x10u;
constexpr std::uint32_t memory_io_mmu_secure_mode_supported = 0x800u;
constexpr std::uint32_t memory_cross_adapter_texture = 0x8000u;
constexpr std::uint32_t memory_cross_adapter_scanout = 0x10000u;
constexpr std::uint32_t presentation_kernel_mode_command_buffer = 0x4u;
constexpr std::uint32_t presentation_software_device_bitmaps = 0x10000000u;
constexpr std::uint32_t flip_independent = 0x10u;
constexpr std::uint32_t misc_independent_vidpn_vsync_control = 0x10u;
constexpr std::uint32_t misc_first_reserved = 0x200u;
constexpr std::uint32_t color_3x4_matrix = 0x4u;
constexpr std::uint32_t color_3x4_wide = 0x8u;
constexpr std::uint32_t color_3x4_high = 0x10u;

constexpr std::uint32_t driver_caps_bytes = 0x250u;
constexpr std::uint32_t physical_adapter_caps_bytes = 0x20u;
constexpr std::uint32_t native_fence_caps_bytes = 0x38u;
constexpr std::uint32_t max_overlay_planes_before_2_0 = 8u;
constexpr std::uint32_t max_overlay_planes = 10u;

bool bit(std::uint32_t word, std::uint32_t mask) { return (word & mask) != 0u; }

}  // namespace

FakeDxgadapter::FakeDxgadapter(MockPlatform &platform, const DdiTable &table)
    : platform_(platform), table_(table), caps_{}, failed_(CapabilityRule::none), queries_(0u) {}

bool FakeDxgadapter::has(DdiSlot slot) const { return table_.routine(slot) != nullptr; }

std::int32_t FakeDxgadapter::refuse(CapabilityRule rule, std::int32_t status)
{
    failed_ = rule;
    return status;
}

std::int32_t FakeDxgadapter::query(void *context, std::int32_t type, void *input, std::uint32_t input_size,
                                   void *output, std::uint32_t output_size)
{
    const auto routine = reinterpret_cast<QueryAdapterInfo>(table_.routine(DdiSlot::DxgkDdiQueryAdapterInfo));
    if (routine == nullptr) return status_revision_mismatch;
    DxgkargQueryAdapterInfoRecord record = {};
    record.Type = type;
    record.pInputData = input;
    record.InputDataSize = input_size;
    record.pOutputData = output;
    record.OutputDataSize = output_size;
    platform_.set_irql(Irql::passive);
    queries_++;
    const std::int32_t status = routine(context, &record);
    if (status != 0 && status != status_not_implemented && status != status_invalid_parameter &&
        status != status_no_memory)
        TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                "QueryAdapterInfo type %d: invalid NTSTATUS 0x%08X", type,
                                                static_cast<unsigned>(status));
    return status;
}

/* Only a WDDMv2 driver is asked, and a failed query leaves defaults rather
 * than failing start; a successful one is held to MultiEngineAware. */
std::int32_t FakeDxgadapter::query_physical_adapter(void *context)
{
    if (caps_.WDDMVersion < wddm_2_0) return 0;
    std::uint32_t index = 0u;
    std::uint8_t  physical[physical_adapter_caps_bytes] = {};
    if (query(context, query_physical_adapter_caps, &index, sizeof index, physical, sizeof physical) >= 0) {
        const std::uint32_t nodes = static_cast<std::uint32_t>(physical[0] | physical[1] << 8u);
        if (caps_.WDDMVersion < wddm_2_5 && nodes > max_nodes_before_2_5)
            return refuse(CapabilityRule::too_many_nodes, status_invalid_parameter);
        if (!bit(caps_.SchedulingCaps, scheduling_multi_engine_aware))
            return refuse(CapabilityRule::multi_engine_aware_missing, status_invalid_parameter);
    }
    if (!bit(caps_.MemoryManagementCaps, memory_io_mmu_secure_mode_supported)) return 0;
    std::uint64_t save_size = 0u;
    const std::int32_t status =
        query(context, query_frame_buffer_save_size, &index, sizeof index, &save_size, sizeof save_size);
    if (status < 0) return refuse(CapabilityRule::frame_buffer_save_size, status);
    if ((save_size & (page_bytes - 1u)) != 0u)
        return refuse(CapabilityRule::frame_buffer_save_size, status_invalid_parameter);
    return 0;
}

std::int32_t FakeDxgadapter::query_after_validation(void *context)
{
    std::uint32_t answer = 0u;
    if (caps_.WDDMVersion >= wddm_3_2)
        static_cast<void>(query(context, query_dirty_bit_tracking_caps, nullptr, 0u, &answer, sizeof answer));
    static_cast<void>(query(context, query_64_bit_only_caps, nullptr, 0u, &answer, sizeof answer));
    if (!bit(caps_.SchedulingCaps, scheduling_native_gpu_fence)) return 0;
    std::uint8_t native_fence_caps[native_fence_caps_bytes] = {};
    const std::int32_t status =
        query(context, query_native_fence_caps, nullptr, 0u, native_fence_caps, sizeof native_fence_caps);
    if (status < 0) return refuse(CapabilityRule::native_fence_caps_query_failed, status);
    return 0;
}

std::int32_t FakeDxgadapter::initialize(void *context, std::uint32_t children)
{
    failed_ = CapabilityRule::none;
    queries_ = 0u;
    std::memset(&caps_, 0, sizeof caps_);
    std::int32_t status = query(context, query_driver_caps, nullptr, 0u, &caps_, driver_caps_bytes);
    if (status < 0) return refuse(CapabilityRule::driver_caps_query_failed, status);
    status = query_physical_adapter(context);
    if (status < 0) return status;
    const CapabilityRule rule = validate(caps_, children);
    if (rule == CapabilityRule::unknown_wddm_version) return refuse(rule, status_revision_mismatch);
    if (rule != CapabilityRule::none) return refuse(rule, status_invalid_parameter);
    return query_after_validation(context);
}

CapabilityRule FakeDxgadapter::validate(const DxgkDriverCapsRecord &caps, std::uint32_t children) const
{
    CapabilityRule rule = validate_version(caps);
    if (rule == CapabilityRule::none) rule = validate_levels(caps);
    if (rule == CapabilityRule::none) rule = validate_hybrid(caps, children);
    if (rule == CapabilityRule::none) rule = validate_ddis(caps);
    if (rule == CapabilityRule::none) rule = validate_overlays(caps);
    if (rule == CapabilityRule::none) rule = validate_color_and_diagnostics(caps);
    if (rule == CapabilityRule::none) rule = validate_full_adapter(caps);
    return rule;
}

CapabilityRule FakeDxgadapter::validate_version(const DxgkDriverCapsRecord &caps) const
{
    if (caps.WDDMVersion >= wddm_2_4 && caps.MiscCaps >= misc_first_reserved)
        return CapabilityRule::misc_caps_reserved;
    if (bit(caps.MiscCaps, misc_independent_vidpn_vsync_control) && !has(DdiSlot::DxgkDdiControlInterrupt3))
        return CapabilityRule::vsync_control_without_control_interrupt3;
    for (const std::int32_t known : known_versions)
        if (caps.WDDMVersion == known) return CapabilityRule::none;
    return CapabilityRule::unknown_wddm_version;
}

CapabilityRule FakeDxgadapter::validate_levels(const DxgkDriverCapsRecord &caps) const
{
    const bool wddm_1_2_features =
        caps.PreemptionCapsGraphicsPreemptionGranularity != 0 || caps.PreemptionCapsComputePreemptionGranularity != 0 ||
        caps.SupportNonVGA != 0u || caps.SupportSmoothRotation != 0u || caps.SupportPerEngineTDR != 0u ||
        bit(caps.PresentationCaps, presentation_software_device_bitmaps) ||
        bit(caps.SchedulingCaps, scheduling_wddm_1_2_bits) || caps.SupportDirectFlip != 0u ||
        caps.SupportRuntimePowerManagement != 0u || caps.SupportSurpriseRemovalInHibernation != 0u;
    if (caps.WDDMVersion < wddm_1_2 && wddm_1_2_features) return CapabilityRule::wddm_1_2_feature_below_1_2;
    const bool wddm_1_3_features = bit(caps.FlipCaps, flip_independent) ||
                                   bit(caps.MemoryManagementCaps, memory_cross_adapter_resource) ||
                                   caps.HybridDiscrete != 0u || caps.MaxOverlayPlanes != 0u;
    if (caps.WDDMVersion < wddm_1_3 && wddm_1_3_features) return CapabilityRule::wddm_1_3_feature_below_1_3;
    if (caps.WDDMVersion < wddm_2_0 && caps.HybridIntegrated != 0u) return CapabilityRule::hybrid_integrated_below_2_0;
    const std::uint32_t memory = caps.MemoryManagementCaps;
    const std::uint32_t tiers = memory & (memory_cross_adapter_resource | memory_cross_adapter_texture);
    if (bit(memory, memory_cross_adapter_scanout) && tiers != (memory_cross_adapter_resource | memory_cross_adapter_texture))
        return CapabilityRule::cross_adapter_tiers;
    if (tiers == memory_cross_adapter_texture) return CapabilityRule::cross_adapter_tiers;
    return CapabilityRule::none;
}

CapabilityRule FakeDxgadapter::validate_hybrid(const DxgkDriverCapsRecord &caps, std::uint32_t children) const
{
    const bool discrete = caps.HybridDiscrete != 0u;
    const bool integrated = caps.HybridIntegrated != 0u;
    if ((discrete || integrated) && !bit(caps.MemoryManagementCaps, memory_cross_adapter_resource))
        return CapabilityRule::hybrid_without_cross_adapter_resource;
    if (discrete && integrated) return CapabilityRule::both_hybrid;
    if (integrated && children == 0u) return CapabilityRule::hybrid_integrated_without_outputs;
    return CapabilityRule::none;
}

CapabilityRule FakeDxgadapter::validate_ddis(const DxgkDriverCapsRecord &caps) const
{
    if (caps.SupportPerEngineTDR != 0u &&
        (!has(DdiSlot::DxgkDdiQueryDependentEngineGroup) || !has(DdiSlot::DxgkDdiQueryEngineStatus) ||
         !has(DdiSlot::DxgkDdiResetEngine)))
        return CapabilityRule::per_engine_tdr_without_ddis;
    if (bit(caps.PresentationCaps, presentation_kernel_mode_command_buffer) && !has(DdiSlot::DxgkDdiRenderKm))
        return CapabilityRule::kernel_mode_command_buffer_without_render_km;
    if (caps.SupportRuntimePowerManagement != 0u &&
        (!has(DdiSlot::DxgkDdiSetPowerComponentFState) || !has(DdiSlot::DxgkDdiPowerRuntimeControlRequest)))
        return CapabilityRule::runtime_power_without_ddis;
    return CapabilityRule::none;
}

CapabilityRule FakeDxgadapter::validate_overlays(const DxgkDriverCapsRecord &caps) const
{
    if (caps.SupportMultiPlaneOverlay == 0u) return CapabilityRule::none;
    if (caps.MaxOverlayPlanes == 0u) return CapabilityRule::overlay_without_planes;
    const std::uint32_t ceiling = caps.WDDMVersion >= wddm_2_2 ? max_overlay_planes : max_overlay_planes_before_2_0;
    const bool wddm_2_0_ten = caps.WDDMVersion >= wddm_2_0 && caps.MaxOverlayPlanes == max_overlay_planes;
    if (caps.MaxOverlayPlanes > ceiling && !wddm_2_0_ten) return CapabilityRule::too_many_overlay_planes;
    const bool set_address = has(DdiSlot::DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay) ||
                             has(DdiSlot::DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay2) ||
                             has(DdiSlot::DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay3);
    const bool check = has(DdiSlot::DxgkDdiCheckMultiPlaneOverlaySupport) ||
                       has(DdiSlot::DxgkDdiCheckMultiPlaneOverlaySupport2) ||
                       has(DdiSlot::DxgkDdiCheckMultiPlaneOverlaySupport3);
    if (!set_address || !check) return CapabilityRule::overlay_without_ddis;
    if (caps.SupportDirectFlip == 0u) return CapabilityRule::overlay_without_direct_flip;
    return CapabilityRule::none;
}

CapabilityRule FakeDxgadapter::validate_color_and_diagnostics(const DxgkDriverCapsRecord &caps) const
{
    if (caps.WDDMVersion >= wddm_2_3) {
        const bool matrix = bit(caps.GammaRampCaps, color_3x4_matrix);
        const bool wide = bit(caps.GammaRampCaps, color_3x4_wide);
        const bool high = bit(caps.GammaRampCaps, color_3x4_high);
        if ((wide && !matrix) || (high && !wide)) return CapabilityRule::color_transform_levels;
        if (matrix && !has(DdiSlot::DxgkDdiSetTargetGamma)) return CapabilityRule::color_transform_without_target_gamma;
    }
    if (caps.WDDMVersion >= wddm_2_4 &&
        has(DdiSlot::DxgkDdiQueryDiagnosticTypesSupport) != has(DdiSlot::DxgkDdiControlDiagnosticReporting))
        return CapabilityRule::diagnostic_ddis_unpaired;
    return CapabilityRule::none;
}

/* IsFullWDDMAdapter: both a render and a display adapter. */
CapabilityRule FakeDxgadapter::validate_full_adapter(const DxgkDriverCapsRecord &caps) const
{
    const bool full = has(DdiSlot::DxgkDdiCreateDevice) && has(DdiSlot::DxgkDdiCommitVidPn);
    if (!full) return CapabilityRule::none;
    if (caps.WDDMVersion >= wddm_1_2 && caps.SupportDirectFlip == 0u)
        return CapabilityRule::wddm_1_2_full_adapter_without_direct_flip;
    if (caps.WDDMVersion >= wddm_1_3 && !bit(caps.FlipCaps, flip_independent))
        return CapabilityRule::wddm_1_3_full_adapter_without_independent_flip;
    return CapabilityRule::none;
}

CapabilityRule              FakeDxgadapter::failed_rule() const { return failed_; }
const DxgkDriverCapsRecord &FakeDxgadapter::driver_caps() const { return caps_; }
std::uint32_t               FakeDxgadapter::queries() const { return queries_; }

}  // namespace av
