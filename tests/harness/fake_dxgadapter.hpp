/* dxgkrnl's DXGADAPTER for one started adapter: the QueryAdapterInfo calls
 * DXGADAPTER::Initialize @ 1401a2818 makes, in order, and the checks it
 * applies to DXGK_DRIVERCAPS (docs/ddi-sequences.md section 2.4). Every query
 * goes through DXGADAPTER::DdiQueryAdapterInfo's rule: a status other than
 * the four it accepts fails the test.
 *
 * Not modelled: registry and feature-database state (ACG, compute class, UMD
 * name, NativeFence enablement), paravirtualized adapters, and the render and
 * display cores Initialize creates after these checks (features 008, 011). */
#ifndef AV_FAKE_DXGADAPTER_HPP
#define AV_FAKE_DXGADAPTER_HPP

#include "../../src/core/ddi/dxgk_driver_caps_record.hpp"
#include "../../src/core/ddi/dxgk_node_metadata_record.hpp"
#include "../../src/core/ddi/dxgk_physical_adapter_caps_record.hpp"
#include "../../src/core/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "../../src/core/ddi_table.hpp"
#include "capability_rule.hpp"
#include "mock_platform.hpp"

#include <cstdint>

namespace av {

class FakeDxgadapter {
public:
    static constexpr std::int32_t status_invalid_parameter = static_cast<std::int32_t>(0xC000000Du);
    static constexpr std::int32_t status_revision_mismatch = static_cast<std::int32_t>(0xC0000059u);

    FakeDxgadapter(MockPlatform &platform, const DdiTable &table);

    /* Initialize's capability queries and checks for an adapter reporting
     * `children` outputs; answers the NTSTATUS adapter start fails with. An
     * adapter with outputs is first asked to prioritize HDR (SetModeBehavior
     * @ 1401a8d0c, DDI interface >= 0x7000): the answer is only traced, but a
     * status other than SUCCESS, NO_MEMORY or NOT_SUPPORTED is logged invalid
     * and a behavior both satisfied and not trips an assertion. */
    std::int32_t initialize(void *context, std::uint32_t children);
    /* The registry value GraphicsDrivers\ValidateWDDMCaps (DXGADAPTER+0xBE0):
     * only with it does Initialize refuse 1.2/1.3 features below their version
     * or a full adapter without DirectFlip and FlipIndependent. Off by default. */
    void         validate_wddm_caps();
    /* DXGADAPTER::DdiQueryAdapterInfo at PASSIVE. */
    std::int32_t query(void *context, std::int32_t type, void *input, std::uint32_t input_size, void *output,
                       std::uint32_t output_size);
    /* ADAPTER_RENDER::CreateRenderCore @ 1401b13d8 over the table and the last
     * DRIVERCAPS: no render DDI at all is a display-only adapter (success,
     * render_adapter() false); some but not all fails with
     * STATUS_REVISION_MISMATCH naming the first missing slot. The later checks
     * CreateRenderCore makes (WDDM 1.2 render features, node metadata) arrive
     * with the features that report those levels. From 2.0 a render adapter
     * that is not compute-only must report FlipOnVSyncMmIo. */
    std::int32_t create_render_core();
    bool         render_adapter() const;
    DdiSlot      missing_render_slot() const;
    /* The DRIVERCAPS checks alone, so a test can craft an answer. */
    CapabilityRule validate(const DxgkDriverCapsRecord &caps, std::uint32_t children) const;

    /* DxgkDdiGetNodeMetadata through the table at PASSIVE. */
    std::int32_t node_metadata(void *context, std::uint32_t node, DxgkNodeMetadataRecord &metadata);
    /* The answer to PHYSICALADAPTERCAPS, or the defaults Initialize derives
     * from DRIVERCAPS when the driver is below 2.0 or does not answer. */
    const DxgkPhysicalAdapterCapsRecord &physical_adapter_caps() const;

    /* The last ControlModeBehavior answer; zero when the slot is NULL and the
     * default answered STATUS_NOT_SUPPORTED. */
    const DxgkargControlModeBehaviorRecord &mode_behavior() const;
    /* DXGK_DISPLAY_DRIVERCAPS_EXTENSION as ADAPTER_DISPLAY::Initialize @ 1403abf44
     * keeps it for a WDDM 2.0 adapter with outputs: the answer, or when the query
     * fails, VirtualModeSupport from 2.2 and nothing before. */
    std::uint32_t display_caps_extension() const;
    CapabilityRule              failed_rule() const;
    const DxgkDriverCapsRecord &driver_caps() const;
    /* Since the last initialize. */
    std::uint32_t               queries() const;

private:
    static constexpr std::uint32_t max_nodes_before_2_5 = 64u;
    static constexpr std::uint32_t page_bytes = 0x1000u;

    std::int32_t   refuse(CapabilityRule rule, std::int32_t status);
    void           set_mode_behavior(void *context);
    std::int32_t   query_physical_adapter(void *context);
    std::int32_t   query_after_validation(void *context, std::uint32_t children);
    CapabilityRule validate_version(const DxgkDriverCapsRecord &caps) const;
    CapabilityRule validate_levels(const DxgkDriverCapsRecord &caps) const;
    CapabilityRule validate_hybrid(const DxgkDriverCapsRecord &caps, std::uint32_t children) const;
    CapabilityRule validate_ddis(const DxgkDriverCapsRecord &caps) const;
    CapabilityRule validate_overlays(const DxgkDriverCapsRecord &caps) const;
    CapabilityRule validate_color_and_diagnostics(const DxgkDriverCapsRecord &caps) const;
    CapabilityRule validate_full_adapter(const DxgkDriverCapsRecord &caps) const;
    bool           has(DdiSlot slot) const;

    std::int32_t render_ddi_rule();

    MockPlatform         &platform_;
    const DdiTable       &table_;
    DxgkDriverCapsRecord  caps_;
    DxgkPhysicalAdapterCapsRecord physical_;
    DxgkargControlModeBehaviorRecord mode_behavior_;
    std::uint32_t         display_caps_extension_;
    bool                  validate_wddm_caps_;
    CapabilityRule        failed_;
    std::uint32_t         queries_;
    bool                  render_adapter_;
    DdiSlot               missing_render_slot_;
};

}  // namespace av

#endif  // AV_FAKE_DXGADAPTER_HPP
