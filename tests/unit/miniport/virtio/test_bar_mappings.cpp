#include "miniport/driver/dxgkrnl_interface.hpp"
#include "miniport/virtio/bar_mappings.hpp"
#include "miniport/pci/pci_base_addresses.hpp"
#include "miniport/pci/resource_list.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/virtio_capability_walk.hpp"
#include "miniport/virtio/virtio_pci_layout.hpp"
#include "miniport/virtio/virtio_region.hpp"
#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

namespace av {

struct BarMappingsProbe {
    explicit BarMappingsProbe(BarMappings &target) : mappings(target) {}
    void set_length(std::uint8_t bar, std::uint64_t length) { mappings.lengths_[bar] = length; }
    BarMappings &mappings;
};

}

using av::BarMappings;
using av::BarMappingsProbe;
using av::DxgkDeviceInfoRecord;
using av::DxgkrnlInterface;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::Irql;
using av::MockPlatform;
using av::PciBaseAddresses;
using av::ResourceList;
using av::Status;
using av::Violation;
using av::VirtioCapabilityWalk;
using av::VirtioPciLayout;
using av::VirtioRegion;

namespace {
class Discovery {
public:
    Discovery() : dxgkrnl_(platform_, function_), interface_(platform_), resources_(platform_) {}

    Status discover()
    {
        Status status = interface_.initialize(&dxgkrnl_.record());
        DxgkDeviceInfoRecord info;
        if (status == Status::ok) status = interface_.get_device_information(info);
        if (status == Status::ok) status = resources_.initialize(info.TranslatedResourceList, ResourceList::max_bytes);
        if (status == Status::ok) status = VirtioCapabilityWalk(function_.config_space(), 256u).walk(layout_);
        return status;
    }

    PciBaseAddresses       bars() { return PciBaseAddresses(function_.config_space(), 256u); }
    MockPlatform          &platform() { return platform_; }
    FakeVirtioGpuFunction &function() { return function_; }
    FakeDxgkrnlAdapter    &dxgkrnl() { return dxgkrnl_; }
    DxgkrnlInterface      &interface() { return interface_; }
    ResourceList          &resources() { return resources_; }
    VirtioPciLayout       &layout() { return layout_; }

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    ResourceList          resources_;
    VirtioPciLayout       layout_;
};

}

TEST(bar_mappings, starts_with_nothing_mapped)
{ Discovery discovery;
  const BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_EQ(mappings.mapped_bars(), 0u);
  CHECK(mappings.address_of(VirtioRegion(4u, 0u, 0x1000u)) == nullptr);
  CHECK(mappings.invariants_hold()); }

TEST(bar_mappings_map, maps_the_one_bar_all_four_structures_share_once)
{ Discovery discovery;
  CHECK_STATUS(discovery.discover(), Status::ok);
  { BarMappings mappings(discovery.platform(), discovery.interface());
    CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::ok);
    CHECK(mappings.invariants_hold());
    CHECK_EQ(mappings.mapped_bars(), 1u);
    CHECK_EQ(discovery.dxgkrnl().maps(), 1u);
    const auto common = static_cast<std::uint8_t *>(mappings.address_of(discovery.layout().common()));
    const auto notify = static_cast<std::uint8_t *>(mappings.address_of(discovery.layout().notify()));
    CHECK(common != nullptr);
    CHECK(notify == common + 0x3000u);
    CHECK_EQ(discovery.platform().read_register_u16(common + 0x12u), 2u);
    CHECK(mappings.address_of(VirtioRegion()) == nullptr);
    CHECK(mappings.address_of(VirtioRegion(4u, 0x3000u, 0x1001u)) == nullptr);
    CHECK(mappings.address_of(VirtioRegion(2u, 0u, 0x10u)) == nullptr);
    CHECK(mappings.address_of(VirtioRegion(4u, 0xFFFFFFFFFFFFF000ull, 0x2000u)) == nullptr); }
  CHECK_EQ(discovery.dxgkrnl().live_mappings(), 0u); }

TEST(bar_mappings_map, a_second_map_is_invalid_state)
{ Discovery discovery;
  CHECK_STATUS(discovery.discover(), Status::ok);
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::ok);
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::invalid_state);
  mappings.unmap_all();
  CHECK_EQ(mappings.mapped_bars(), 0u);
  mappings.unmap_all();
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::ok); }

TEST(bar_mappings_map, a_bar_with_no_resource_is_not_found_and_maps_nothing)
{ Discovery discovery;
  discovery.function().remove_virtio_bar_resource();
  CHECK_STATUS(discovery.discover(), Status::ok);
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::not_found);
  CHECK_EQ(mappings.mapped_bars(), 0u); }

TEST(bar_mappings_map, a_resource_too_short_for_a_structure_is_malformed)
{ Discovery discovery;
  discovery.function().set_virtio_bar_length(0x3800u);
  CHECK_STATUS(discovery.discover(), Status::ok);
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::malformed);
  CHECK_EQ(discovery.dxgkrnl().live_mappings(), 0u); }

TEST(bar_mappings_map, an_unassigned_or_io_bar_propagates)
{ Discovery discovery;
  CHECK_STATUS(discovery.discover(), Status::ok);
  discovery.function().config_space()[0x20u] = 0x01u;
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::not_supported); }

TEST(bar_mappings_map, a_structure_whose_end_wraps_is_malformed)
{ Discovery discovery;
  CHECK_STATUS(discovery.discover(), Status::ok);
  const VirtioRegion wrapping(4u, 0xFFFFFFFFFFFFF000ull, 0x2000u);
  const VirtioRegion &common = discovery.layout().common();
  const VirtioPciLayout layout(common, wrapping, common, common, 4u, nullptr, nullptr, 0u);
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), layout), Status::malformed);
  CHECK_EQ(discovery.dxgkrnl().live_mappings(), 0u); }

TEST(bar_mappings_map, a_bar_larger_than_one_mapping_is_too_large)
{ Discovery discovery;
  CHECK_STATUS(discovery.discover(), Status::ok);
  av::FakeResourceList huge;
  huge.add_large_memory(FakeVirtioGpuFunction::virtio_bar_address, 0x0800u, 1u);
  CHECK_STATUS(discovery.resources().initialize(huge.bytes(), huge.size()), Status::ok);
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::too_large); }

TEST(bar_mappings_map, a_refused_mapping_propagates_and_unwinds)
{ Discovery discovery;
  CHECK_STATUS(discovery.discover(), Status::ok);
  discovery.dxgkrnl().refuse_map(1u, 0xC000009Au);
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::no_space);
  CHECK_EQ(mappings.mapped_bars(), 0u); }

TEST(bar_mappings_map, a_region_on_a_second_bar_maps_that_bar_too_and_unwinds_both)
{ Discovery discovery;
  CHECK_STATUS(discovery.discover(), Status::ok);
  const VirtioRegion on_bar_1(1u, 0u, 0x1000u);
  const VirtioRegion &common = discovery.layout().common();
  const VirtioPciLayout layout(common, common, on_bar_1, common, 4u, nullptr, nullptr, 0u);
  { BarMappings mappings(discovery.platform(), discovery.interface());
    CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), layout), Status::ok);
    CHECK_EQ(mappings.mapped_bars(), 2u); }
  CHECK_EQ(discovery.dxgkrnl().live_mappings(), 0u);
  discovery.dxgkrnl().refuse_map(2u, 0xC0000017u);
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), layout), Status::no_memory);
  CHECK_EQ(discovery.dxgkrnl().live_mappings(), 0u); }

TEST(bar_mappings_map, a_bar_index_past_five_is_an_invalid_parameter)
{ Discovery discovery;
  CHECK_STATUS(discovery.discover(), Status::ok);
  const VirtioRegion on_bar_6(6u, 0u, 0x1000u);
  const VirtioRegion &common = discovery.layout().common();
  const VirtioPciLayout layout(common, common, common, on_bar_6, 4u, nullptr, nullptr, 0u);
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), layout), Status::invalid_parameter);
  CHECK(mappings.address_of(on_bar_6) == nullptr);
  CHECK_EQ(discovery.dxgkrnl().live_mappings(), 0u); }

TEST(bar_mappings_unmap_all, a_refused_unmap_is_logged_and_forgotten)
{ Discovery discovery;
  CHECK_STATUS(discovery.discover(), Status::ok);
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::ok);
  discovery.dxgkrnl().refuse_unmap(0xC0000001u);
  const std::uint32_t logged = discovery.platform().logged();
  mappings.unmap_all();
  CHECK_EQ(discovery.platform().logged(), logged + 1u);
  CHECK_EQ(mappings.mapped_bars(), 0u); }

TEST(bar_mappings_map, is_passive_only)
{ Discovery discovery;
  CHECK_STATUS(discovery.discover(), Status::ok);
  BarMappings mappings(discovery.platform(), discovery.interface());
  discovery.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::ok);
  discovery.platform().set_irql(Irql::passive);
  CHECK(discovery.platform().claim(Violation::irql_ceiling));
  while (discovery.platform().claim(Violation::irql_ceiling)) {}
  mappings.unmap_all(); }

TEST(bar_mappings_invariants, a_length_without_a_mapping_is_detected)
{ Discovery discovery;
  BarMappings mappings(discovery.platform(), discovery.interface());
  BarMappingsProbe(mappings).set_length(3u, 0x1000u);
  CHECK(!mappings.invariants_hold());
  BarMappingsProbe(mappings).set_length(3u, 0u); }

TEST(bar_mappings_map_region, maps_the_doorbell_bar_after_the_structures)
{ Discovery discovery;
  discovery.function().add_doorbell_region();
  CHECK_STATUS(discovery.discover(), Status::ok);
  const VirtioRegion doorbells = discovery.layout().shared_memory(FakeVirtioGpuFunction::doorbell_region_id);
  CHECK(doorbells.present());
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map_region(discovery.bars(), discovery.resources(), doorbells), Status::invalid_state);
  CHECK_EQ(discovery.dxgkrnl().live_mappings(), 0u);
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::ok);
  CHECK(mappings.address_of(doorbells) == nullptr);
  CHECK_STATUS(mappings.map_region(discovery.bars(), discovery.resources(), doorbells), Status::ok);
  CHECK(mappings.invariants_hold());
  CHECK_EQ(mappings.mapped_bars(), 2u);
  auto *const page_one = static_cast<std::uint8_t *>(mappings.address_of(doorbells)) + 0x1000u;
  discovery.platform().write_register_u32(page_one, 1u);
  CHECK_EQ(discovery.function().doorbells().rings(1u), 1u);
  CHECK_STATUS(mappings.map_region(discovery.bars(), discovery.resources(), doorbells), Status::ok);
  CHECK_EQ(discovery.dxgkrnl().live_mappings(), 2u);
  mappings.unmap_all();
  CHECK_EQ(discovery.dxgkrnl().live_mappings(), 0u); }

TEST(bar_mappings_map_region, a_region_past_its_resource_is_refused_and_the_structures_stay)
{ Discovery discovery;
  discovery.function().add_doorbell_region();
  CHECK_STATUS(discovery.discover(), Status::ok);
  BarMappings mappings(discovery.platform(), discovery.interface());
  CHECK_STATUS(mappings.map(discovery.bars(), discovery.resources(), discovery.layout()), Status::ok);
  const VirtioRegion oversized(0u, 0u, 0x100000u);
  CHECK(mappings.map_region(discovery.bars(), discovery.resources(), oversized) != Status::ok);
  CHECK_EQ(mappings.mapped_bars(), 1u);
  CHECK(mappings.invariants_hold()); }
