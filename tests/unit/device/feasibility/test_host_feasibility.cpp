/* What the guest is told this host has, and nothing else: host memory reaches
 * the guest only through a hostmem window (docs/host-memory.md), so a device
 * with none offers no host-visible memory. */
#include "core/support/status.hpp"
#include "device/feasibility/host_feasibility.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/capset_info.hpp"

#include <cstdint>

using av::CapsetInfo;
using av::HostFeasibility;
using av::MockHost;

TEST(host_feasibility_features, a_device_with_a_hostmem_window_offers_host_visible_memory_alone)
{ MockHost host;
  host.set_hostmem_size(0x100000u);
  HostFeasibility feasibility(host);
  CHECK_EQ(feasibility.features(), CapsetInfo::feature_host_visible_memory);
  /* Asked each time, of the window the host has now. */
  host.set_hostmem_size(0u);
  CHECK_EQ(feasibility.features(), 0u); }

TEST(host_feasibility_features, a_device_with_no_hostmem_window_offers_nothing)
{ MockHost        host;
  HostFeasibility feasibility(host);
  CHECK_EQ(feasibility.features(), 0u);
  host.set_hostmem_size(4096u);
  CHECK_EQ(feasibility.features(), CapsetInfo::feature_host_visible_memory); }

/* The fence path reports into the one probe the device keeps: what it was
 * told is what that probe judges. */
TEST(host_feasibility, reports_interrupts_through_its_one_probe)
{ MockHost        host;
  HostFeasibility feasibility(host);
  CHECK_STATUS(feasibility.interrupts().initialize(), av::Status::ok);
  feasibility.interrupts().raised(0u);
  feasibility.interrupts().raised(1u);
  feasibility.interrupts().acknowledged(2u, 1000u);
  feasibility.interrupts().finish();
  CHECK(feasibility.interrupts().verdict().ran());
  CHECK(feasibility.interrupts().verdict().passed());
  CHECK_EQ(feasibility.interrupts().verdict().measured(), 1000u); }
