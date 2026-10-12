/* The clock the pacing steps judge FIFO by: a blank period is the counter's
 * ticks between two blanks on the avgpu adapter, on a device the clock makes
 * and gives back; no adapter, or one that cannot be waited on, is no period. */
#include "core/support/status.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/probe/vblank_clock.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstdint>

using av::AdapterSearch;
using av::AvgpuAdapters;
using av::MockKmt;
using av::PciIdentity;
using av::VblankClock;

namespace {

const PciIdentity avgpu(AvgpuAdapters::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                        AvgpuAdapters::virtio_vendor, AvgpuAdapters::av_subsystem);
constexpr std::uint32_t wddm_3_2 = 3200u;
constexpr std::uint64_t blank = 166667u;

}  // namespace

TEST(vblank_clock, a_period_is_the_ticks_between_two_blanks_on_a_device_given_back)
{ MockKmt kmt;
  kmt.add_adapter(avgpu, wddm_3_2);
  kmt.set_blank_period(blank);
  VblankClock clock(kmt);
  const std::uint64_t before = clock.now();
  CHECK_EQ(clock.period(), blank);
  CHECK(clock.now() >= before + 2u * blank);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(vblank_clock, no_avgpu_adapter_is_no_period)
{ MockKmt kmt;
  kmt.set_blank_period(blank);
  VblankClock clock(kmt);
  CHECK_EQ(clock.period(), 0u); }

TEST(vblank_clock, a_blank_that_cannot_be_waited_for_is_no_period_and_the_device_still_goes)
{ MockKmt kmt;
  kmt.add_adapter(avgpu, wddm_3_2);
  kmt.set_blank_period(blank);
  VblankClock first(kmt);
  kmt.fail_at(MockKmt::Call::wait_for_vertical_blank, 1u, 0xC0000001u);
  CHECK_EQ(first.period(), 0u);
  CHECK_EQ(kmt.open_objects(), 0u);
  kmt.fail_at(MockKmt::Call::wait_for_vertical_blank, 2u, 0xC0000001u);
  CHECK_EQ(first.period(), 0u);
  CHECK_EQ(kmt.open_objects(), 0u);
  kmt.fail_at(MockKmt::Call::create_device, 1u, 0xC0000001u);
  CHECK_EQ(first.period(), 0u); }
