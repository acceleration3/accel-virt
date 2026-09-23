#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_search.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

using av::AdapterSearch;
using av::MockKmt;
using av::PciIdentity;
using av::Status;

namespace {

const PciIdentity rvk(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
const PciIdentity stock(0x1AF4u, 0x1050u, 0x1AF4u, 0x1100u);
const PciIdentity basic_render(0x1414u, 0x008Cu, 0u, 0u);
/* virtio-gpu-rvk-pci presenting the host's GPUs, and a real card of the same
 * model passed through beside it. */
const PciIdentity on_radeon(0x1002u, 0x7550u, 0x1AF4u, 0x5256u);
const PciIdentity on_raphael(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u);
const PciIdentity real_radeon(0x1002u, 0x7550u, 0x1002u, 0x0E3Bu);

}  // namespace

TEST(adapter_search, keeps_the_first_match_and_closes_every_other_adapter)
{ MockKmt kmt;
  kmt.add_adapter(basic_render, 3100u);
  kmt.add_adapter(rvk, 3200u);
  kmt.add_adapter(rvk, 3200u);
  kmt.add_adapter(stock, 1300u);
  { AdapterSearch search(kmt);
    CHECK(!search.found());
    CHECK_EQ(search.adapter(), 0u);
    CHECK_STATUS(search.find(rvk), Status::ok);
    CHECK(search.found());
    CHECK_EQ(kmt.open_objects(), 1u);
    CHECK_EQ(kmt.calls(MockKmt::Call::query_identity), 2u);   /* none after the match */
    CHECK_STATUS(search.find(rvk), Status::invalid_state);
    CHECK_STATUS(search.close(), Status::ok);
    CHECK(!search.found());
    CHECK_STATUS(search.close(), Status::ok);   /* nothing held */
    CHECK_STATUS(search.find(rvk), Status::ok); }
  CHECK_EQ(kmt.open_objects(), 0u);   /* the destructor closed it */
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(adapter_search, reports_an_enumeration_failure_and_no_match)
{ MockKmt kmt;
  kmt.add_adapter(stock, 1300u);
  AdapterSearch search(kmt);
  kmt.fail_at(MockKmt::Call::enumerate_adapters, 1u, 0xC0000001u);
  CHECK_STATUS(search.find(rvk), Status::invalid_state);
  CHECK_STATUS(search.find(rvk), Status::not_found);
  CHECK_EQ(kmt.open_objects(), 0u);
  kmt.add_adapter(rvk, 3200u);
  kmt.fail_at(MockKmt::Call::query_identity, 1u, 0xC0000001u);   /* the stock adapter's */
  CHECK_STATUS(search.find(rvk), Status::ok);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(adapter_search, hostile_enumerations_claiming_too_many_or_a_zero_handle)
{ MockKmt kmt;
  kmt.add_adapter(stock, 1300u);
  kmt.add_adapter(rvk, 3200u);
  AdapterSearch search(kmt);
  kmt.claim_adapters(AdapterSearch::max_adapters + 40u);
  CHECK_STATUS(search.find(rvk), Status::ok);
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_STATUS(search.close(), Status::ok);
  kmt.zero_next(MockKmt::Call::enumerate_adapters);
  CHECK_STATUS(search.find(stock), Status::not_found);   /* the zero handle was stock's slot */
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(adapter_search, finds_the_device_by_its_subsystem_whatever_gpu_it_presents)
{ MockKmt kmt;
  kmt.add_adapter(real_radeon, 3200u);
  kmt.add_adapter(stock, 1300u);
  kmt.add_adapter(on_radeon, 3200u);
  kmt.add_adapter(on_raphael, 3200u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find(rvk), Status::ok);
  CHECK_EQ(kmt.calls(MockKmt::Call::query_identity), 3u);   /* the card and stock were passed over */
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(adapter_search, a_real_card_of_the_same_model_is_not_the_device)
{ MockKmt kmt;
  kmt.add_adapter(real_radeon, 3200u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find(rvk), Status::not_found);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }
