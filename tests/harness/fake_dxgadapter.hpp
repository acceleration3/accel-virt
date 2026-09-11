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
     * `children` outputs; answers the NTSTATUS adapter start fails with. */
    std::int32_t initialize(void *context, std::uint32_t children);
    /* DXGADAPTER::DdiQueryAdapterInfo at PASSIVE. */
    std::int32_t query(void *context, std::int32_t type, void *input, std::uint32_t input_size, void *output,
                       std::uint32_t output_size);
    /* The DRIVERCAPS checks alone, so a test can craft an answer. */
    CapabilityRule validate(const DxgkDriverCapsRecord &caps, std::uint32_t children) const;

    CapabilityRule              failed_rule() const;
    const DxgkDriverCapsRecord &driver_caps() const;
    /* Since the last initialize. */
    std::uint32_t               queries() const;

private:
    static constexpr std::uint32_t max_nodes_before_2_5 = 64u;
    static constexpr std::uint32_t page_bytes = 0x1000u;

    std::int32_t   refuse(CapabilityRule rule, std::int32_t status);
    std::int32_t   query_physical_adapter(void *context);
    std::int32_t   query_after_validation(void *context);
    CapabilityRule validate_version(const DxgkDriverCapsRecord &caps) const;
    CapabilityRule validate_levels(const DxgkDriverCapsRecord &caps) const;
    CapabilityRule validate_hybrid(const DxgkDriverCapsRecord &caps, std::uint32_t children) const;
    CapabilityRule validate_ddis(const DxgkDriverCapsRecord &caps) const;
    CapabilityRule validate_overlays(const DxgkDriverCapsRecord &caps) const;
    CapabilityRule validate_color_and_diagnostics(const DxgkDriverCapsRecord &caps) const;
    CapabilityRule validate_full_adapter(const DxgkDriverCapsRecord &caps) const;
    bool           has(DdiSlot slot) const;

    MockPlatform         &platform_;
    const DdiTable       &table_;
    DxgkDriverCapsRecord  caps_;
    CapabilityRule        failed_;
    std::uint32_t         queries_;
};

}  // namespace av

#endif  // AV_FAKE_DXGADAPTER_HPP
