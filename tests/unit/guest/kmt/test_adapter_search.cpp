#include "core/support/status.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstdint>

using av::AdapterSearch;
using av::MockKmt;
using av::PciIdentity;
using av::Status;

namespace {

const PciIdentity stock(0x1AF4u, 0x1050u, 0x1AF4u, 0x1100u);
const PciIdentity basic_render(0x1414u, 0x008Cu, 0u, 0u);
const PciIdentity on_radeon(0x1002u, 0x7550u, 0x1AF4u, 0x5256u);
const PciIdentity on_raphael(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u);
const PciIdentity real_radeon(0x1002u, 0x7550u, 0x1002u, 0x0E3Bu);

std::uint64_t luid_of(MockKmt &kmt, const AdapterSearch &search)
{
    std::uint64_t luid = 0u;
    CHECK_STATUS(kmt.adapter_luid(search.adapter(), luid), Status::ok);
    return luid;
}

}

TEST(adapter_search, keeps_one_avgpu_adapter_and_closes_every_other)
{ MockKmt kmt;
  kmt.add_adapter(basic_render, 3100u);
  kmt.add_adapter(on_radeon, 3200u);
  kmt.add_adapter(on_raphael, 3200u);
  kmt.add_adapter(stock, 1300u);
  { AdapterSearch search(kmt);
    CHECK(!search.found());
    CHECK_EQ(search.adapter(), 0u);
    CHECK_STATUS(search.find_avgpu(0u), Status::ok);
    CHECK(search.found());
    CHECK_EQ(kmt.open_objects(), 1u);
    CHECK_STATUS(search.find_avgpu(0u), Status::invalid_state);
    CHECK_STATUS(search.close(), Status::ok);
    CHECK(!search.found());
    CHECK_STATUS(search.close(), Status::ok);
    CHECK_STATUS(search.find_avgpu(0u), Status::ok); }
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(adapter_search, reports_an_enumeration_failure_and_no_avgpu_adapter)
{ MockKmt kmt;
  kmt.add_adapter(real_radeon, 3200u);
  kmt.add_adapter(stock, 1300u);
  AdapterSearch search(kmt);
  kmt.fail_at(MockKmt::Call::enumerate_adapters, 1u, 0xC0000001u);
  CHECK_STATUS(search.find_avgpu(0u), Status::invalid_state);
  CHECK_STATUS(search.find_avgpu(0u), Status::not_found);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(adapter_search, finds_avgpu_by_luid)
{ MockKmt kmt;
  kmt.add_adapter(real_radeon, 3200u);
  kmt.add_adapter(on_radeon, 3200u);
  kmt.add_adapter(on_raphael, 3200u);
  kmt.set_adapter_luid(0u, 0x11u);
  kmt.set_adapter_luid(1u, 0x22u);
  kmt.set_adapter_luid(2u, 0x33u);
  { AdapterSearch search(kmt);
    CHECK_STATUS(search.find_avgpu(0x33u), Status::ok);
    CHECK_EQ(luid_of(kmt, search), 0x33u); }
  { AdapterSearch search(kmt);
    CHECK_STATUS(search.find_avgpu(0x11u), Status::not_found); }
  { AdapterSearch search(kmt);
    CHECK_STATUS(search.find_avgpu(0x99u), Status::not_found); }
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(adapter_search, avgpu_asked_for_no_luid_is_the_adapter_the_primary_display_is_on)
{ MockKmt kmt;
  kmt.add_adapter(on_raphael, 3200u);
  kmt.add_adapter(on_radeon, 3200u);
  kmt.set_adapter_luid(0u, 0x11u);
  kmt.set_adapter_luid(1u, 0x22u);
  { AdapterSearch search(kmt);
    CHECK_STATUS(search.find_avgpu(0u), Status::ok);
    CHECK_EQ(luid_of(kmt, search), 0x11u); }
  kmt.set_primary_display_luid(0x22u);
  { AdapterSearch search(kmt);
    CHECK_STATUS(search.find_avgpu(0u), Status::ok);
    CHECK_EQ(luid_of(kmt, search), 0x22u); }
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }
