/* US1: whether memory the host GPU exports reaches the guest through the
 * hostmem region. Every refusal the host can give is scripted here, and every
 * path must leave the region and the GPU as it found them. */
#include "core/support/status.hpp"
#include "device/probe/host_memory_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

using av::HostMemoryProbe;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace {

constexpr std::int32_t out_of_device_memory = -2;   /* VK_ERROR_OUT_OF_DEVICE_MEMORY */

}  // namespace

TEST(host_memory_probe_run, a_device_with_no_hostmem_region_asks_the_host_for_nothing)
{ MockHost host;
  HostMemoryProbe probe(host);
  probe.run();
  CHECK(probe.result().ran());
  CHECK(!probe.result().passed());
  CHECK_STATUS(probe.result().reason(), Status::not_supported);
  CHECK_EQ(host.live_gpu_memory(), 0u);
  CHECK_EQ(host.live_placements(), 0u); }

TEST(host_memory_probe_run, memory_that_reaches_the_region_and_reads_back_passes)
{ MockHost host;
  host.set_hostmem_size(HostMemoryProbe::probe_bytes);
  HostMemoryProbe probe(host);
  probe.run();
  CHECK(probe.result().passed());
  CHECK_EQ(probe.result().measured(), HostMemoryProbe::probe_bytes);
  CHECK_EQ(probe.result().errno_value(), 0);
  /* Nothing of the probe's is left in the region or on the GPU. */
  CHECK_EQ(host.live_gpu_memory(), 0u);
  CHECK_EQ(host.live_placements(), 0u);
  CHECK_EQ(host.unclaimed_violations(), 0u); }

TEST(host_memory_probe_run, a_host_that_refuses_the_export_is_recorded_with_its_own_number)
{ MockHost host;
  host.set_hostmem_size(HostMemoryProbe::probe_bytes);
  host.refuse_export_at(1u, out_of_device_memory);
  HostMemoryProbe probe(host);
  probe.run();
  CHECK(!probe.result().passed());
  CHECK_STATUS(probe.result().reason(), Status::not_supported);
  CHECK_EQ(probe.result().errno_value(), out_of_device_memory);
  CHECK_EQ(host.live_gpu_memory(), 0u); }

TEST(host_memory_probe_run, a_placement_the_host_refuses_still_releases_the_export)
{ MockHost host;
  host.set_hostmem_size(HostMemoryProbe::probe_bytes);
  host.fail_placement_at(1u);
  HostMemoryProbe probe(host);
  probe.run();
  CHECK(!probe.result().passed());
  CHECK_STATUS(probe.result().reason(), Status::no_memory);
  CHECK_EQ(host.live_gpu_memory(), 0u);
  CHECK_EQ(host.live_placements(), 0u); }

/* The mapping existed and lied, which is worse than a refusal: it would have
 * shown the guest bytes the GPU never wrote. */
TEST(host_memory_probe_run, memory_that_does_not_read_back_is_malformed)
{ MockHost host;
  host.set_hostmem_size(HostMemoryProbe::probe_bytes);
  host.corrupt_hostmem_read_at(1u);
  HostMemoryProbe probe(host);
  probe.run();
  CHECK(!probe.result().passed());
  CHECK_STATUS(probe.result().reason(), Status::malformed);
  CHECK_EQ(probe.result().errno_value(), 0);
  CHECK_EQ(host.live_gpu_memory(), 0u);
  CHECK_EQ(host.live_placements(), 0u); }

/* Constitution V: the host is outside, and what it answers is not trusted. A
 * handle with no mapping behind it, and a region that cannot be read at all,
 * are both refusals rather than a probe that carries on with a null. */
TEST(host_memory_probe_run, a_host_that_answers_a_handle_with_no_mapping_is_refused)
{ MockHost host;
  host.set_hostmem_size(HostMemoryProbe::probe_bytes);
  host.empty_export_at(1u);
  HostMemoryProbe probe(host);
  probe.run();
  CHECK(!probe.result().passed());
  CHECK_STATUS(probe.result().reason(), Status::not_supported);
  CHECK_EQ(host.live_gpu_memory(), 0u);
  CHECK_EQ(host.live_placements(), 0u); }

TEST(host_memory_probe_run, a_region_the_host_cannot_read_back_is_refused)
{ MockHost host;
  host.set_hostmem_size(HostMemoryProbe::probe_bytes);
  host.refuse_hostmem_read_at(1u);
  HostMemoryProbe probe(host);
  probe.run();
  CHECK(!probe.result().passed());
  CHECK_STATUS(probe.result().reason(), Status::not_found);
  CHECK_EQ(host.live_gpu_memory(), 0u);
  CHECK_EQ(host.live_placements(), 0u); }

TEST(host_memory_probe_run, answers_once)
{ MockHost host;
  host.set_hostmem_size(HostMemoryProbe::probe_bytes);
  HostMemoryProbe probe(host);
  probe.run();
  CHECK(probe.result().passed());
  probe.run();
  CHECK(probe.result().passed());
  CHECK_EQ(host.live_gpu_memory(), 0u);
  CHECK_EQ(host.unclaimed_violations(), 0u); }

TEST(host_memory_probe, runs_on_the_main_loop)
{ MockHost host;
  host.set_hostmem_size(HostMemoryProbe::probe_bytes);
  HostMemoryProbe probe(host);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread([](void *context) { static_cast<HostMemoryProbe *>(context)->run(); },
                                 &probe, thread),
               Status::ok);
  host.join_thread(thread);
  CHECK(host.claim(HostViolation::main_loop_required));
  while (host.claim(HostViolation::main_loop_required)) {} }
