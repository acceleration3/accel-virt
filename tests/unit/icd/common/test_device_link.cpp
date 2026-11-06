#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/common/device_link.hpp"

#include <cstdint>
#include <cstring>

using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::icd::DeviceLink;

namespace {

const PciIdentity avgpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);

std::uint32_t added(MockKmt &kmt)
{
    kmt.add_adapter(avgpu, 3200u);
    std::uint32_t adapter = 0u, count = 0u;
    CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
    return adapter;
}

}

TEST(device_link, a_link_nobody_holds_is_sound_and_letting_go_of_it_stays_at_zero)
{ MockKmt kmt;
  DeviceLink link(kmt);
  CHECK(link.invariants_hold());
  CHECK_EQ(link.users(), 0u);
  CHECK(!link.lost());
  CHECK(link.let_go());
  CHECK_EQ(link.users(), 0u);
  CHECK(link.invariants_hold()); }

TEST(device_link, the_last_user_to_let_go_is_told_and_the_ones_before_it_are_not)
{ MockKmt kmt;
  DeviceLink link(kmt);
  CHECK_STATUS(link.open(added(kmt)), Status::ok);
  link.hold();
  link.hold();
  CHECK_EQ(link.users(), 2u);
  CHECK(link.invariants_hold());
  CHECK(!link.let_go());
  CHECK(link.let_go());
  CHECK_EQ(link.users(), 0u);
  CHECK(link.invariants_hold()); }

TEST(device_link, a_held_link_whose_device_is_not_open_is_unsound_and_a_loss_is_kept_and_reported)
{ MockKmt kmt;
  DeviceLink link(kmt);
  link.hold();
  CHECK(!link.invariants_hold());
  CHECK(link.let_go());
  link.lose();
  CHECK(link.lost());
  char bytes[128];
  av::StateWriter writer(bytes, sizeof bytes);
  link.report(writer);
  CHECK(std::strstr(writer.dump(), "users=0 lost=true") != nullptr); }
