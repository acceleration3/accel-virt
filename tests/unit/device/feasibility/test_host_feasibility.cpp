/* US4: what the probes decided is what the guest is told, and nothing else. */
#include "core/support/status.hpp"
#include "device/feasibility/host_feasibility.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/capset_info.hpp"

#include <cstdint>

using av::CapsetInfo;
using av::HostFeasibility;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace {

constexpr std::uint64_t window = 0x100000u;

/* A host on which both memory probes can pass. */
void with_everything(MockHost &host)
{
    host.set_hostmem_size(0x100000u);
    host.add_guest_ram(window, 0x100000u);
}

}  // namespace

TEST(host_feasibility_decide, a_host_where_both_probes_pass_offers_both_paths)
{ MockHost host;
  with_everything(host);
  HostFeasibility feasibility(host);
  feasibility.decide(window);
  CHECK_EQ(feasibility.features(), CapsetInfo::feature_host_visible_memory |
                                   CapsetInfo::feature_gpu_exported_memory |
                                   CapsetInfo::feature_guest_memory_import);
  CHECK_EQ(host.unclaimed_violations(), 0u); }

/* Constitution VI: a bit is what a probe proved on this host, not what the
 * driver would like to be true. */
TEST(host_feasibility_features, carries_no_bit_whose_probe_did_not_pass)
{ MockHost host;
  with_everything(host);
  host.refuse_export_at(1u, -2);
  HostFeasibility exported(host);
  exported.decide(window);
  CHECK_EQ(exported.features(), CapsetInfo::feature_host_visible_memory |
                                CapsetInfo::feature_guest_memory_import);

  MockHost second;
  with_everything(second);
  second.refuse_import_at(1u, 0);
  HostFeasibility imported(second);
  imported.decide(window);
  CHECK_EQ(imported.features(), CapsetInfo::feature_host_visible_memory |
                                CapsetInfo::feature_gpu_exported_memory); }

TEST(host_feasibility_features, a_device_with_no_hostmem_region_offers_nothing)
{ MockHost host;
  HostFeasibility feasibility(host);
  feasibility.decide(window);
  CHECK_EQ(feasibility.features(), 0u); }

TEST(host_feasibility_decide, decides_once)
{ MockHost host;
  with_everything(host);
  HostFeasibility feasibility(host);
  feasibility.decide(window);
  const std::uint64_t first = feasibility.features();
  host.refuse_export_at(1u, -2);
  feasibility.decide(window);
  CHECK_EQ(feasibility.features(), first);
  CHECK_EQ(host.live_gpu_memory(), 0u); }

/* The one line per probe the device logs, so a cleared bit says why from the
 * guest's own console. */
TEST(host_feasibility_decide, says_what_each_probe_found)
{ MockHost host;
  with_everything(host);
  host.refuse_import_at(1u, -22);
  HostFeasibility feasibility(host);
  feasibility.decide(window);
  CHECK_EQ(host.logged(), 2u); }

TEST(host_feasibility, decides_on_the_main_loop)
{ MockHost host;
  with_everything(host);
  HostFeasibility feasibility(host);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread([](void *context) { static_cast<HostFeasibility *>(context)->decide(window); },
                                 &feasibility, thread),
               Status::ok);
  host.join_thread(thread);
  CHECK(host.claim(HostViolation::main_loop_required));
  while (host.claim(HostViolation::main_loop_required)) {} }
