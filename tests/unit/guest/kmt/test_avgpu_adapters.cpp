#include "core/support/status.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstdint>

using av::AvgpuAdapters;
using av::MockKmt;
using av::PciIdentity;
using av::Status;

namespace {

const PciIdentity stock(0x1AF4u, 0x1050u, 0x1AF4u, 0x1100u);
const PciIdentity basic_render(0x1414u, 0x008Cu, 0u, 0u);
const PciIdentity on_radeon(0x1002u, 0x7550u, 0x1AF4u, 0x5256u);
const PciIdentity on_raphael(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u);
const PciIdentity on_geforce(0x10DEu, 0x2C05u, 0x1AF4u, 0x5256u);
const PciIdentity real_radeon(0x1002u, 0x7550u, 0x1002u, 0x0E3Bu);

}

TEST(avgpu_adapters, holds_every_avgpu_adapter_and_closes_every_other)
{ MockKmt kmt;
  kmt.add_adapter(basic_render, 3100u);
  kmt.add_adapter(on_radeon, 3200u);
  kmt.add_adapter(real_radeon, 3200u);
  kmt.add_adapter(on_raphael, 3200u);
  kmt.add_adapter(stock, 1300u);
  kmt.set_adapter_luid(1u, 0x22u);
  kmt.set_adapter_luid(3u, 0x33u);
  { AvgpuAdapters adapters(kmt);
    CHECK_EQ(adapters.count(), 0u);
    CHECK_STATUS(adapters.learn(), Status::ok);
    CHECK(adapters.invariants_hold());
    CHECK_EQ(adapters.count(), 2u);
    CHECK_EQ(kmt.open_objects(), 2u);
    CHECK_EQ(adapters.luid(0u), 0x22u);
    CHECK_EQ(adapters.luid(1u), 0x33u);
    CHECK(adapters.identity(0u).same_as(on_radeon));
    CHECK(adapters.identity(1u).same_as(on_raphael));
    CHECK(adapters.adapter(0u) != 0u && adapters.adapter(1u) != 0u);
    CHECK(adapters.adapter(0u) != adapters.adapter(1u));
    CHECK_EQ(adapters.adapter(2u), 0u);
    CHECK_EQ(adapters.luid(2u), 0u);
    CHECK(!adapters.identity(2u).same_subsystem_as(on_radeon));
    CHECK(!adapters.primary(0u)); }
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(avgpu_adapters, the_primary_displays_adapter_comes_first_and_the_rest_keep_their_order)
{ MockKmt kmt;
  kmt.add_adapter(on_raphael, 3200u);
  kmt.add_adapter(on_geforce, 3200u);
  kmt.add_adapter(on_radeon, 3200u);
  kmt.set_adapter_luid(0u, 0x11u);
  kmt.set_adapter_luid(1u, 0x22u);
  kmt.set_adapter_luid(2u, 0x33u);
  kmt.set_primary_display_luid(0x33u);
  AvgpuAdapters adapters(kmt);
  CHECK_STATUS(adapters.learn(), Status::ok);
  CHECK(adapters.invariants_hold());
  CHECK_EQ(adapters.count(), 3u);
  CHECK_EQ(adapters.luid(0u), 0x33u);
  CHECK_EQ(adapters.luid(1u), 0x11u);
  CHECK_EQ(adapters.luid(2u), 0x22u);
  CHECK(adapters.identity(0u).same_as(on_radeon));
  CHECK(adapters.identity(1u).same_as(on_raphael));
  CHECK(adapters.primary(0u));
  CHECK(!adapters.primary(1u));
  std::uint64_t luid = 0u;
  CHECK_STATUS(kmt.adapter_luid(adapters.adapter(0u), luid), Status::ok);
  CHECK_EQ(luid, 0x33u);
  kmt.set_primary_display_luid(0x77u);
  CHECK_STATUS(adapters.learn(), Status::ok);
  CHECK_EQ(adapters.luid(0u), 0x11u);
  CHECK(!adapters.primary(0u));
  CHECK_EQ(kmt.open_objects(), 3u);
  adapters.close();
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(avgpu_adapters, finds_an_adapter_by_its_luid_and_none_by_zero)
{ MockKmt kmt;
  kmt.add_adapter(on_radeon, 3200u);
  kmt.add_adapter(on_raphael, 3200u);
  kmt.set_adapter_luid(0u, 0x22u);
  kmt.set_adapter_luid(1u, 0x33u);
  AvgpuAdapters adapters(kmt);
  CHECK_STATUS(adapters.learn(), Status::ok);
  CHECK_EQ(adapters.index_of(0x33u), 1u);
  CHECK_EQ(adapters.index_of(0x22u), 0u);
  CHECK_EQ(adapters.index_of(0x99u), 2u);
  CHECK_EQ(adapters.index_of(0u), 2u); }

TEST(avgpu_adapters, a_taken_adapter_is_left_open_for_the_taker)
{ MockKmt kmt;
  kmt.add_adapter(on_radeon, 3200u);
  kmt.add_adapter(on_raphael, 3200u);
  std::uint32_t taken = 0u;
  { AvgpuAdapters adapters(kmt);
    CHECK_STATUS(adapters.learn(), Status::ok);
    taken = adapters.take(1u);
    CHECK(taken != 0u);
    CHECK(adapters.invariants_hold());
    CHECK_EQ(adapters.adapter(1u), 0u);
    CHECK_EQ(adapters.take(1u), 0u);
    CHECK_EQ(adapters.take(2u), 0u);
    CHECK_EQ(adapters.count(), 2u);
    CHECK(adapters.luid(1u) != 0u); }
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_STATUS(kmt.close_adapter(taken), Status::ok);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(avgpu_adapters, an_adapter_whose_luid_windows_will_not_give_is_held_without_one)
{ MockKmt kmt;
  kmt.add_adapter(on_radeon, 3200u);
  kmt.fail_at(MockKmt::Call::adapter_luid, 1u, 0xC0000001u);
  AvgpuAdapters adapters(kmt);
  CHECK_STATUS(adapters.learn(), Status::ok);
  CHECK_EQ(adapters.count(), 1u);
  CHECK_EQ(adapters.luid(0u), 0u);
  CHECK_EQ(adapters.index_of(0u), 1u);
  CHECK(adapters.invariants_hold()); }

TEST(avgpu_adapters, a_refused_enumeration_or_identity_holds_nothing_of_it)
{ MockKmt kmt;
  kmt.add_adapter(on_radeon, 3200u);
  kmt.add_adapter(on_raphael, 3200u);
  AvgpuAdapters adapters(kmt);
  kmt.fail_at(MockKmt::Call::enumerate_adapters, 1u, 0xC0000001u);
  CHECK_STATUS(adapters.learn(), Status::invalid_state);
  CHECK_EQ(adapters.count(), 0u);
  kmt.fail_at(MockKmt::Call::query_identity, 1u, 0xC0000001u);
  CHECK_STATUS(adapters.learn(), Status::ok);
  CHECK_EQ(adapters.count(), 1u);
  CHECK(adapters.identity(0u).same_as(on_raphael));
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(avgpu_adapters, hostile_enumerations_claiming_too_many_or_a_zero_handle)
{ MockKmt kmt;
  kmt.add_adapter(on_radeon, 3200u);
  kmt.add_adapter(on_raphael, 3200u);
  AvgpuAdapters adapters(kmt);
  kmt.claim_adapters(AvgpuAdapters::max_adapters + 40u);
  CHECK_STATUS(adapters.learn(), Status::ok);
  CHECK(adapters.count() <= AvgpuAdapters::max_adapters);
  CHECK(adapters.invariants_hold());
  kmt.zero_next(MockKmt::Call::enumerate_adapters);
  CHECK_STATUS(adapters.learn(), Status::ok);
  CHECK_EQ(adapters.count(), 1u);
  CHECK(adapters.identity(0u).same_as(on_raphael));
  adapters.close();
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }
