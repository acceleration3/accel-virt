#ifndef AV_FAKE_DXGADAPTER_HPP
#define AV_FAKE_DXGADAPTER_HPP

#include "miniport/ddi/dxgk_driver_caps_record.hpp"
#include "miniport/ddi/dxgk_node_metadata_record.hpp"
#include "miniport/ddi/dxgk_physical_adapter_caps_record.hpp"
#include "miniport/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "miniport/driver/ddi_table.hpp"
#include "harness/dxgkrnl/capability_rule.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class FakeDxgadapter {
public:
    static constexpr std::int32_t status_invalid_parameter = static_cast<std::int32_t>(0xC000000Du);
    static constexpr std::int32_t status_revision_mismatch = static_cast<std::int32_t>(0xC0000059u);

    FakeDxgadapter(MockPlatform &platform, const DdiTable &table);

    std::int32_t initialize(void *context, std::uint32_t children);
    void         validate_wddm_caps();
    std::int32_t query(void *context, std::int32_t type, void *input, std::uint32_t input_size, void *output,
                       std::uint32_t output_size);
    std::int32_t create_render_core();
    bool         render_adapter() const;
    DdiSlot      missing_render_slot() const;
    CapabilityRule validate(const DxgkDriverCapsRecord &caps, std::uint32_t children) const;

    std::int32_t node_metadata(void *context, std::uint32_t node, DxgkNodeMetadataRecord &metadata);
    const DxgkPhysicalAdapterCapsRecord &physical_adapter_caps() const;

    const DxgkargControlModeBehaviorRecord &mode_behavior() const;
    std::uint32_t display_caps_extension() const;
    CapabilityRule              failed_rule() const;
    const DxgkDriverCapsRecord &driver_caps() const;
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

}

#endif
