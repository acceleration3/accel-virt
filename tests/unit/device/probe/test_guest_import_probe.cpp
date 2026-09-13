/* US2: whether the host GPU can address guest pages directly. The probe asks
 * about a window of guest RAM, and only a window the host maps directly and the
 * GPU reads back as written counts. */
#include "core/support/status.hpp"
#include "device/probe/guest_import_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

using av::GuestImportProbe;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace {

constexpr std::uint64_t window = 0x100000u;   /* where the probe's guest RAM starts */

/* Guest RAM the host maps directly, big enough for the probe's window. */
void with_ram(MockHost &host)
{
    host.add_guest_ram(window, GuestImportProbe::probe_bytes);
}

}  // namespace

TEST(guest_import_probe_run, nothing_mapped_there_is_not_found)
{ MockHost host;
  GuestImportProbe probe(host);
  probe.run(window);
  CHECK(probe.result().ran());
  CHECK(!probe.result().passed());
  CHECK_STATUS(probe.result().reason(), Status::not_found);
  CHECK_EQ(host.live_imports(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u); }

/* An import over a bounce buffer proves nothing: the GPU would read a copy the
 * guest cannot see. */
TEST(guest_import_probe_run, a_bounce_buffer_is_refused_without_asking_the_host_to_import)
{ MockHost host;
  host.add_bounce_region(window, GuestImportProbe::probe_bytes);
  GuestImportProbe probe(host);
  probe.run(window);
  CHECK(!probe.result().passed());
  CHECK_STATUS(probe.result().reason(), Status::not_supported);
  CHECK_EQ(host.live_imports(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(guest_import_probe_run, a_window_the_gpu_reads_back_as_written_passes)
{ MockHost host;
  with_ram(host);
  GuestImportProbe probe(host);
  probe.run(window);
  CHECK(probe.result().passed());
  CHECK_EQ(probe.result().measured(), GuestImportProbe::probe_bytes);
  CHECK_EQ(probe.result().errno_value(), 0);
  CHECK_EQ(host.live_imports(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK_EQ(host.unclaimed_violations(), 0u); }

/* A host with no VK_EXT_external_memory_host refuses with no number of its own;
 * that is a clean answer, and the copy fallback is what the guest then uses. */
TEST(guest_import_probe_run, a_host_that_cannot_import_is_a_clean_refusal)
{ MockHost host;
  with_ram(host);
  host.refuse_import_at(1u, 0);
  GuestImportProbe probe(host);
  probe.run(window);
  CHECK(!probe.result().passed());
  CHECK_STATUS(probe.result().reason(), Status::not_supported);
  CHECK_EQ(probe.result().errno_value(), 0);
  CHECK_EQ(host.live_guest_mappings(), 0u); }

/* What a guest reading through a copy would get instead, so 041 knows what it
 * is choosing between rather than guessing. */
TEST(guest_import_probe_run, a_refused_import_measures_the_copy_the_fallback_would_do)
{ MockHost host;
  with_ram(host);
  host.refuse_import_at(1u, 0);
  host.advance_clock_per_read(1000u);   /* the copy takes a microsecond */
  GuestImportProbe probe(host);
  probe.run(window);
  CHECK(!probe.result().passed());
  /* A megabyte in a microsecond: a terabyte a second, in bytes per second. */
  CHECK_EQ(probe.result().measured(), GuestImportProbe::probe_bytes * 1000000000u / 1000u);
  CHECK_EQ(host.live_host_memory(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(guest_import_probe_run, a_copy_the_host_has_no_memory_for_measures_nothing)
{ MockHost host;
  with_ram(host);
  host.refuse_import_at(1u, 0);
  host.fail_host_memory_at(1u);
  GuestImportProbe probe(host);
  probe.run(window);
  CHECK(!probe.result().passed());
  CHECK_EQ(probe.result().measured(), 0u);
  CHECK_EQ(host.live_host_memory(), 0u); }

/* A clock that does not move cannot answer a rate, and dividing by it would be
 * worse than saying nothing. */
TEST(guest_import_probe_run, a_copy_too_fast_for_the_clock_measures_nothing)
{ MockHost host;
  with_ram(host);
  host.refuse_import_at(1u, 0);
  GuestImportProbe probe(host);
  probe.run(window);
  CHECK(!probe.result().passed());
  CHECK_EQ(probe.result().measured(), 0u); }

TEST(guest_import_probe_run, an_import_that_fails_with_a_number_keeps_it)
{ MockHost host;
  with_ram(host);
  host.refuse_import_at(1u, -22);   /* EINVAL: the window did not meet the alignment */
  GuestImportProbe probe(host);
  probe.run(window);
  CHECK(!probe.result().passed());
  CHECK_EQ(probe.result().errno_value(), -22); }

TEST(guest_import_probe_run, a_gpu_that_reads_back_something_else_is_malformed)
{ MockHost host;
  with_ram(host);
  host.corrupt_import_read_at(1u);
  GuestImportProbe probe(host);
  probe.run(window);
  CHECK(!probe.result().passed());
  CHECK_STATUS(probe.result().reason(), Status::malformed);
  CHECK_EQ(host.live_imports(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(guest_import_probe_run, answers_once)
{ MockHost host;
  with_ram(host);
  GuestImportProbe probe(host);
  probe.run(window);
  CHECK(probe.result().passed());
  probe.run(window);
  CHECK(probe.result().passed());
  CHECK_EQ(host.live_imports(), 0u); }

TEST(guest_import_probe, runs_on_the_main_loop)
{ MockHost host;
  with_ram(host);
  GuestImportProbe probe(host);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread([](void *context) { static_cast<GuestImportProbe *>(context)->run(window); },
                                 &probe, thread),
               Status::ok);
  host.join_thread(thread);
  CHECK(host.claim(HostViolation::main_loop_required));
  while (host.claim(HostViolation::main_loop_required)) {} }
