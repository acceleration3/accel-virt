#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/vendor/vendor_adapters.hpp"
#include "proto/escape/umd_adapter_info.hpp"

#include <cstdint>
#include <cstring>
#include <initializer_list>

using av::MockKmt;
using av::PciIdentity;
using av::PciLocation;
using av::Status;
using av::UmdAdapterInfo;
using av::vendor::VendorAdapter;
using av::vendor::VendorAdapters;

namespace {
const PciIdentity on_raphael(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u, 0xC9u);
const PciIdentity on_geforce(0x10DEu, 0x2684u, 0x1AF4u, 0x5256u, 0xA1u);
const PciIdentity basic_render(0x1414u, 0x008Cu, 0u, 0u);
const PciIdentity real_radeon(0x1002u, 0x7550u, 0x1002u, 0x0E3Bu);
constexpr std::uint64_t version = 0x0000002300050000ull;

}

TEST(vendor_adapters, learns_every_avgpu_adapter_and_only_those_with_each_fact_windows_reports)
{ MockKmt kmt;
  kmt.add_adapter(basic_render, 3100u);
  kmt.add_adapter(on_raphael, 3200u);
  kmt.add_adapter(real_radeon, 3200u);
  kmt.add_adapter(on_geforce, 3200u);
  kmt.describe_adapter(1u, PciLocation(0u, 17u, 0u), "AMD Radeon(TM) Graphics", version);
  kmt.describe_adapter(3u, PciLocation(3u, 0u, 1u), "NVIDIA GeForce RTX 4080", version);
  kmt.set_adapter_features(1u, UmdAdapterInfo::feature_integrated_gpu);
  kmt.set_segment_sizes(512ull << 20u, 0u, 4ull << 30u);
  kmt.set_adapter_luid(1u, 0x51u);
  kmt.set_primary_display_luid(0x51u);
  VendorAdapters adapters(kmt);
  CHECK_STATUS(adapters.learn(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(adapters.count_of(0x1002u), 1u);
  CHECK_EQ(adapters.count_of(0x10DEu), 1u);
  CHECK_EQ(adapters.count_of(0x1414u), 0u);
  const VendorAdapter *const amd = adapters.nth_of(0x1002u, 0u);
  CHECK(amd != nullptr);
  CHECK_EQ(amd->facts().luid(), 0x51u);
  CHECK_EQ(amd->facts().identity().device(), 0x13C0u);
  CHECK_EQ(amd->facts().identity().revision(), 0xC9u);
  CHECK_EQ(amd->facts().location().device(), 17u);
  CHECK(std::strcmp(amd->facts().name(), "AMD Radeon(TM) Graphics") == 0);
  CHECK_EQ(amd->facts().driver_version(), version);
  CHECK_EQ(amd->facts().dedicated_video_bytes(), 512ull << 20u);
  CHECK_EQ(amd->facts().shared_system_bytes(), 4ull << 30u);
  CHECK(amd->facts().integrated());
  CHECK(amd->primary());
  const VendorAdapter *const nvidia = adapters.nth_of(0x10DEu, 0u);
  CHECK(nvidia != nullptr);
  CHECK(!nvidia->facts().integrated());
  CHECK(!nvidia->primary());
  CHECK_EQ(nvidia->facts().location().bus(), 3u);
  CHECK_EQ(nvidia->facts().location().function(), 1u);
  CHECK(adapters.nth_of(0x1002u, 1u) == nullptr);
  CHECK(adapters.nth_of(0x8086u, 0u) == nullptr); }

TEST(vendor_adapters, an_adapter_a_fact_of_which_windows_will_not_give_is_left_out)
{ for (const MockKmt::Call refused : {MockKmt::Call::adapter_luid, MockKmt::Call::query_adapter_location,
                                     MockKmt::Call::query_adapter_name, MockKmt::Call::query_umd_driver_version,
                                     MockKmt::Call::query_segment_sizes}) {
      MockKmt kmt;
      kmt.add_adapter(on_raphael, 3200u);
      kmt.add_adapter(on_raphael, 3200u);
      kmt.fail_at(refused, refused == MockKmt::Call::adapter_luid ? 3u : 1u, MockKmt::invalid_parameter);
      VendorAdapters adapters(kmt);
      CHECK_STATUS(adapters.learn(), Status::ok);
      CHECK_EQ(adapters.count_of(0x1002u), 1u);
      CHECK_EQ(kmt.open_objects(), 0u);
  } }

TEST(vendor_adapters, a_refused_enumeration_is_the_answer_and_leaves_nothing_learnt)
{ MockKmt kmt;
  kmt.add_adapter(on_raphael, 3200u);
  VendorAdapters adapters(kmt);
  CHECK_STATUS(adapters.learn(), Status::ok);
  CHECK_EQ(adapters.count_of(0x1002u), 1u);
  kmt.fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  CHECK(adapters.learn() != Status::ok);
  CHECK_EQ(adapters.count_of(0x1002u), 0u);
  CHECK(adapters.nth_of(0x1002u, 0u) == nullptr); }

TEST(vendor_adapters, no_integrated_bit_reads_discrete_and_no_primary_display_is_on_none)
{ MockKmt kmt;
  kmt.add_adapter(on_raphael, 3200u);
  kmt.set_primary_display_luid(0u);
  VendorAdapters adapters(kmt);
  CHECK_STATUS(adapters.learn(), Status::ok);
  const VendorAdapter *const amd = adapters.nth_of(0x1002u, 0u);
  CHECK(amd != nullptr);
  CHECK(!amd->facts().integrated());
  CHECK(!amd->primary()); }

TEST(vendor_adapters, more_adapters_than_it_holds_a_zero_handle_and_no_identity_are_each_passed_over)
{ MockKmt kmt;
  kmt.add_adapter(on_raphael, 3200u);
  kmt.add_adapter(on_geforce, 3200u);
  kmt.claim_adapters(VendorAdapters::max_adapters + 4u);
  VendorAdapters adapters(kmt);
  CHECK_STATUS(adapters.learn(), Status::ok);
  CHECK_EQ(adapters.count_of(0x1002u), 1u);
  CHECK_EQ(adapters.count_of(0x10DEu), 1u);
  CHECK_EQ(kmt.open_objects(), 0u);
  MockKmt few;
  few.add_adapter(on_raphael, 3200u);
  few.add_adapter(on_raphael, 3200u);
  few.add_adapter(on_raphael, 3200u);
  few.zero_next(MockKmt::Call::enumerate_adapters);
  few.fail_at(MockKmt::Call::query_identity, 1u, MockKmt::invalid_parameter);
  VendorAdapters some(few);
  CHECK_STATUS(some.learn(), Status::ok);
  CHECK_EQ(some.count_of(0x1002u), 1u);
  CHECK_EQ(few.open_objects(), 0u); }

TEST(vendor_adapters, a_refused_features_query_reads_discrete)
{ MockKmt kmt;
  kmt.add_adapter(on_raphael, 3200u);
  kmt.set_adapter_features(0u, UmdAdapterInfo::feature_integrated_gpu);
  kmt.fail_at(MockKmt::Call::query_adapter_features, 1u, MockKmt::invalid_parameter);
  VendorAdapters adapters(kmt);
  CHECK_STATUS(adapters.learn(), Status::ok);
  const VendorAdapter *const amd = adapters.nth_of(0x1002u, 0u);
  CHECK(amd != nullptr);
  CHECK(!amd->facts().integrated());
  CHECK_STATUS(adapters.learn(), Status::ok);
  CHECK(adapters.nth_of(0x1002u, 0u)->facts().integrated()); }
