#include "device/host/host.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/gpu_identity.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

using av::GpuIdentity;
using av::Host;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace {

struct Counter {
    std::atomic<std::uint32_t> value{0u};
};

void bump(void *counter) { static_cast<Counter *>(counter)->value++; }

struct Rendezvous {
    Host         *host = nullptr;
    void         *event = nullptr;
    Counter       ran;
    std::uint32_t order[4] = {};
    std::uint32_t recorded = 0u;
};

void wait_then_report(void *context)
{
    auto *const rendezvous = static_cast<Rendezvous *>(context);
    rendezvous->host->require_worker("wait_then_report");
    rendezvous->host->wait_event(rendezvous->event);
    rendezvous->ran.value++;
    rendezvous->host->run_on_main_loop(&bump, &rendezvous->ran);
}

void record_first(void *context) { auto *r = static_cast<Rendezvous *>(context); r->order[r->recorded++] = 1u; }
void record_second(void *context) { auto *r = static_cast<Rendezvous *>(context); r->order[r->recorded++] = 2u; }

void touch_main_loop_state(void *host) { static_cast<Host *>(host)->require_main_loop("touch_main_loop_state"); }

int memfd(std::uint64_t size)
{
    const int fd = memfd_create("test memory", MFD_CLOEXEC);
    if (fd >= 0 && ftruncate(fd, static_cast<off_t>(size)) != 0) return -1;
    return fd;
}

}

TEST(mock_host_start_thread, runs_the_entry_on_another_thread_that_hops_back_to_the_main_loop)
{ MockHost mock; Host &host = mock;
  Rendezvous rendezvous;
  rendezvous.host = &host;
  CHECK_STATUS(host.create_event(rendezvous.event), Status::ok);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(&wait_then_report, &rendezvous, thread), Status::ok);
  CHECK_EQ(mock.live_threads(), 1u);
  host.signal_event(rendezvous.event);
  CHECK(mock.wait_for_main_loop_work(1u, 5000u));
  CHECK_EQ(mock.run_main_loop(), 1u);
  CHECK_EQ(rendezvous.ran.value.load(), 2u);
  host.join_thread(thread);
  host.destroy_event(rendezvous.event);
  CHECK_EQ(mock.live_threads(), 0u);
  CHECK_EQ(mock.live_events(), 0u); }

TEST(mock_host_run_on_main_loop, runs_in_call_order)
{ MockHost mock;
  Rendezvous rendezvous;
  mock.run_on_main_loop(&record_first, &rendezvous);
  mock.run_on_main_loop(&record_second, &rendezvous);
  CHECK(mock.wait_for_main_loop_work(2u, 0u));
  CHECK(!mock.wait_for_main_loop_work(3u, 10u));
  CHECK_EQ(mock.run_main_loop(), 2u);
  CHECK_EQ(rendezvous.order[0], 1u);
  CHECK_EQ(rendezvous.order[1], 2u);
  CHECK_EQ(mock.run_main_loop(), 0u); }

TEST(mock_host_events, a_signal_before_the_wait_is_kept_and_many_signals_wake_once)
{ MockHost mock;
  void *event = nullptr;
  CHECK_STATUS(mock.create_event(event), Status::ok);
  mock.signal_event(event);
  mock.signal_event(event);
  mock.wait_event(event);
  mock.destroy_event(event); }

TEST(mock_host_mutexes, lock_and_unlock_and_catch_misuse)
{ MockHost mock;
  void *mutex = nullptr;
  CHECK_STATUS(mock.create_mutex(mutex), Status::ok);
  mock.lock_mutex(mutex);
  mock.destroy_mutex(mutex);
  CHECK(mock.claim(HostViolation::held_mutex));
  CHECK_STATUS(mock.create_mutex(mutex), Status::ok);
  mock.unlock_mutex(mutex);
  CHECK(mock.claim(HostViolation::held_mutex));
  mock.lock_mutex(mutex);
  mock.unlock_mutex(mutex);
  mock.destroy_mutex(mutex);
  CHECK_EQ(mock.live_mutexes(), 0u); }

TEST(mock_host_mutexes, a_try_takes_only_a_free_mutex)
{ MockHost mock;
  void *mutex = nullptr;
  CHECK_STATUS(mock.create_mutex(mutex), Status::ok);
  CHECK(mock.try_lock_mutex(mutex));
  CHECK(!mock.try_lock_mutex(mutex));
  mock.unlock_mutex(mutex);
  mock.hold_mutex_elsewhere(mutex, true);
  CHECK(!mock.try_lock_mutex(mutex));
  mock.hold_mutex_elsewhere(mutex, false);
  CHECK(mock.try_lock_mutex(mutex));
  mock.unlock_mutex(mutex);
  mock.destroy_mutex(mutex);
  int stranger = 0;
  CHECK(!mock.try_lock_mutex(&stranger));
  CHECK(mock.claim(HostViolation::unknown_handle));
  mock.hold_mutex_elsewhere(&stranger, true);
  CHECK(mock.claim(HostViolation::unknown_handle));
  CHECK_EQ(mock.live_mutexes(), 0u); }

TEST(mock_host, unknown_handles_are_violations)
{ MockHost mock;
  int stranger = 0;
  mock.join_thread(&stranger);
  mock.destroy_event(&stranger);
  mock.signal_event(&stranger);
  mock.wait_event(&stranger);
  mock.destroy_mutex(&stranger);
  mock.lock_mutex(&stranger);
  mock.unlock_mutex(&stranger);
  static_cast<void>(mock.gpu_identity(0u));
  for (int count = 0; count < 8; count++) CHECK(mock.claim(HostViolation::unknown_handle));
  CHECK(!mock.claim(HostViolation::unknown_handle));
  CHECK_EQ(mock.unclaimed_violations(), 0u); }

TEST(mock_host, the_contract_is_checked_on_both_sides)
{ MockHost mock;
  mock.require_worker("main loop running worker work");
  CHECK(mock.claim(HostViolation::worker_required));
  void *thread = nullptr;
  CHECK_STATUS(mock.start_thread(&touch_main_loop_state, &mock, thread), Status::ok);
  mock.join_thread(thread);
  CHECK(mock.claim(HostViolation::main_loop_required));
  CHECK_EQ(std::strlen(mock.last_log()), 0u);
  mock.log("once");
  CHECK_EQ(mock.logged(), 1u);
  CHECK(std::strcmp(mock.last_log(), "once") == 0); }

TEST(mock_host, every_create_can_be_failed_and_the_catalog_answers_what_was_added)
{ MockHost mock;
  void *handle = &mock;
  mock.fail_thread_start_at(1u);
  CHECK_STATUS(mock.start_thread(&bump, nullptr, handle), Status::no_memory);
  CHECK(handle == nullptr);
  mock.fail_event_at(2u);
  void *first = nullptr;
  CHECK_STATUS(mock.create_event(first), Status::ok);
  CHECK_STATUS(mock.create_event(handle), Status::no_memory);
  mock.destroy_event(first);
  mock.fail_mutex_at(1u);
  CHECK_STATUS(mock.create_mutex(handle), Status::no_memory);
  const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {0xAB, 1, 2, 3};
  mock.add_gpu(uuid);
  mock.add_gpu(GpuIdentity(0x1002u, 0x744Cu, GpuIdentity::integrated_gpu, uuid, 1u, 2u, "second"));
  CHECK_EQ(mock.gpu_count(), 2u);
  CHECK(std::memcmp(mock.gpu_identity(0u).uuid(), uuid, sizeof uuid) == 0);
  CHECK(std::strcmp(mock.gpu_identity(0u).name(), "mock gpu") == 0);
  CHECK_EQ(mock.gpu_identity(1u).vendor_id(), 0x1002u); }

TEST(mock_host_memory, allocations_are_tracked_and_can_fail)
{ MockHost mock;
  void *const block = mock.allocate(16u, av::Pool::paged);
  CHECK(block != nullptr);
  CHECK_EQ(mock.live_allocations(), 1u);
  mock.release(block);
  mock.release(block);
  CHECK(mock.claim(HostViolation::unknown_handle));
  mock.fail_allocation_at(1u);
  CHECK(mock.allocate(16u, av::Pool::paged) == nullptr); }

TEST(mock_host_map_guest, maps_ram_up_to_a_boundary_and_bounces_the_rest)
{ MockHost mock;
  mock.add_guest_ram(0x1000u, 0x4000u);
  mock.split_guest_memory_at(0x3000u);
  mock.add_bounce_region(0x10000u, 0x1000u);
  mock.guest_ram(0x2000u)[0] = 0x5A;
  std::uint64_t length = 0x3000u;
  bool direct = false;
  void *const ram = mock.map_guest(0x2000u, length, direct);
  CHECK(direct);
  CHECK_EQ(length, 0x1000u);
  CHECK_EQ(static_cast<std::uint8_t *>(ram)[0], 0x5Au);
  length = 0x100u;
  void *const bounce = mock.map_guest(0x10000u, length, direct);
  CHECK(!direct);
  CHECK(bounce != nullptr);
  CHECK(mock.guest_ram(0x10000u) == nullptr);
  CHECK(mock.guest_ram(0x90000u) == nullptr);
  length = 0x1000u;
  CHECK(mock.map_guest(0x90000u, length, direct) == nullptr);
  CHECK_EQ(length, 0u);
  CHECK_EQ(mock.live_guest_mappings(), 2u);
  mock.unmap_guest(ram, 0x1000u);
  mock.unmap_guest(bounce, 0x100u);
  mock.unmap_guest(ram, 0x1000u);
  CHECK(mock.claim(HostViolation::unknown_handle));
  CHECK_EQ(mock.live_guest_mappings(), 0u); }

TEST(mock_host_hostmem, a_placement_maps_the_descriptor_where_the_guest_reads_it)
{ MockHost mock;
  mock.set_hostmem_size(0x10000u);
  CHECK_EQ(mock.hostmem_size(), 0x10000u);
  const int memory = memfd(0x2000u);
  auto *const executor = static_cast<std::uint8_t *>(
      mmap(nullptr, 0x2000u, PROT_READ | PROT_WRITE, MAP_SHARED, memory, 0));
  executor[0x1234] = 0x5A;
  CHECK_STATUS(mock.place_in_hostmem(memory, 0x2000u, 0x4000u), Status::ok);
  auto *const guest = static_cast<std::uint8_t *>(mock.placed_at(0x4000u));
  CHECK(guest != nullptr);
  CHECK(mock.placed_at(0x5000u) == nullptr);
  CHECK_EQ(guest[0x1234], 0x5Au);
  guest[0x10] = 0xA5;
  CHECK_EQ(executor[0x10], 0xA5u);
  CHECK_EQ(mock.live_placements(), 1u);
  CHECK_EQ(mock.placements(), 1u);
  CHECK_EQ(mock.last_placement().fd, memory);
  CHECK_EQ(mock.last_placement().size, 0x2000u);
  CHECK_EQ(mock.last_placement().offset, 0x4000u);
  mock.remove_from_hostmem(0x4000u, 0x2000u);
  CHECK_EQ(mock.live_placements(), 0u);
  CHECK(mock.placed_at(0x4000u) == nullptr);
  CHECK_EQ(mock.placements(), 1u);
  munmap(executor, 0x2000u);
  close(memory); }

TEST(mock_host_hostmem, refuses_what_qemu_could_not_map_and_catches_misuse)
{ MockHost mock;
  mock.set_hostmem_size(0x10000u);
  const int memory = memfd(0x2000u);
  CHECK_STATUS(mock.place_in_hostmem(memory, 0x3000u, 0u), Status::no_memory);
  CHECK_STATUS(mock.place_in_hostmem(memory, 0u, 0u), Status::no_memory);
  int ends[2] = {-1, -1};
  CHECK_EQ(pipe(ends), 0);
  CHECK_STATUS(mock.place_in_hostmem(ends[0], 0x1000u, 0u), Status::no_memory);
  close(ends[0]);
  close(ends[1]);
  CHECK_STATUS(mock.place_in_hostmem(ends[0], 0x1000u, 0u), Status::invalid_parameter);
  CHECK(mock.claim(HostViolation::unknown_handle));
  mock.fail_placement_at(1u);
  CHECK_STATUS(mock.place_in_hostmem(memory, 0x2000u, 0u), Status::no_memory);
  CHECK_EQ(mock.placements(), 0u);
  CHECK_STATUS(mock.place_in_hostmem(memory, 0x2000u, 0u), Status::ok);
  CHECK_STATUS(mock.place_in_hostmem(memory, 0x2000u, 0x1000u), Status::ok);
  CHECK(mock.claim(HostViolation::overlapping_placement));
  mock.remove_from_hostmem(0u, 0x1000u);
  CHECK(mock.claim(HostViolation::unknown_handle));
  CHECK_EQ(mock.live_placements(), 2u);
  mock.remove_from_hostmem(0u, 0x2000u);
  mock.remove_from_hostmem(0x1000u, 0x2000u);
  CHECK_EQ(mock.live_placements(), 0u);
  close(memory); }

TEST(mock_host_descriptors, counts_what_it_handed_out_and_catches_a_double_close)
{ MockHost mock;
  const int counted = mock.open_descriptor();
  CHECK(counted >= 0);
  CHECK(mock.descriptor_open(counted));
  CHECK_EQ(mock.live_descriptors(), 1u);
  const int elsewhere = memfd(0x1000u);
  mock.close_descriptor(elsewhere);
  CHECK(!mock.descriptor_open(elsewhere));
  CHECK_EQ(mock.live_descriptors(), 1u);
  mock.close_descriptor(counted);
  CHECK(!mock.descriptor_open(counted));
  CHECK_EQ(mock.live_descriptors(), 0u);
  mock.close_descriptor(counted);
  CHECK(mock.claim(HostViolation::unknown_handle));
  CHECK(!mock.descriptor_open(-1)); }

TEST(mock_host_scanout, a_head_keeps_the_mapping_it_reads_and_refuses_none)
{ MockHost mock;
  mock.add_display_heads(1u);
  const std::uint8_t blob[64] = {};
  const MockHost::Plane plane = {8u, 2u, 0u, 0u, 8u, 2u, 32u, 0u, 0x34325241u};
  CHECK_STATUS(mock.show_scanout_plane(0u, nullptr, plane), Status::not_supported);
  CHECK(mock.claim(HostViolation::scanout_misuse));
  CHECK_STATUS(mock.show_scanout_plane(0u, blob, plane), Status::ok);
  CHECK(mock.head(0u).shown);
  CHECK(mock.head(0u).blob == blob);
  CHECK_EQ(mock.head(0u).plane.stride, 32u);
  CHECK_STATUS(mock.show_scanout_plane(1u, blob, plane), Status::not_supported);
  CHECK(!mock.claim(HostViolation::scanout_misuse));
  mock.hide_scanout(0u);
  CHECK(!mock.head(0u).shown);
  CHECK(mock.head(0u).blob == nullptr);
  CHECK_EQ(mock.live_scanouts(), 0u); }

TEST(mock_host_requests, answers_once_and_knows_the_base_class_resources)
{ MockHost mock;
  const std::uint8_t bytes[] = {7, 8};
  av::MockRequest request(bytes, sizeof bytes);
  std::uint8_t read[4] = {};
  CHECK_EQ(mock.read_request(&request, 0u, read, sizeof read), 2u);
  CHECK_EQ(read[1], 8u);
  const std::uint8_t response[] = {0x00, 0x11, 0x00, 0x00};
  mock.respond(&request, response, sizeof response);
  CHECK(request.answered());
  CHECK_EQ(request.response_type(), 0x1100u);
  CHECK_EQ(request.response_size(), 4u);
  CHECK_EQ(request.response_u32(4u), 0u);
  mock.respond(&request, response, sizeof response);
  CHECK(mock.claim(HostViolation::second_response));
  mock.add_resource(3u);
  CHECK(mock.resource_exists(3u));
  CHECK(!mock.resource_exists(4u)); }

TEST(mock_host_map_guest, a_translator_answers_before_guest_ram)
{ MockHost mock;
  static std::uint8_t other[64];
  mock.add_guest_ram(0x1000u, 0x1000u);
  mock.translate_guest_with([](void *, std::uint64_t address, std::uint64_t length) -> void * {
      return address >= 0x9000u && address + length <= 0x9040u ? other + (address - 0x9000u) : nullptr;
  }, nullptr);
  std::uint64_t length = 16u;
  bool direct = false;
  void *const translated = mock.map_guest(0x9010u, length, direct);
  CHECK(translated == other + 16);
  CHECK(direct);
  CHECK_EQ(length, 16u);
  length = 0x1000u;
  void *const ram = mock.map_guest(0x1000u, length, direct);
  CHECK(ram != nullptr);
  CHECK_EQ(mock.live_guest_mappings(), 2u);
  mock.unmap_guest(translated, 16u);
  mock.unmap_guest(ram, 0x1000u);
  CHECK_EQ(mock.live_guest_mappings(), 0u); }

TEST(mock_host_doorbells, a_connected_doorbell_wakes_its_event_and_misuse_is_caught)
{ MockHost mock;
  void *event = nullptr;
  void *other = nullptr;
  CHECK_STATUS(mock.create_event(event), Status::ok);
  CHECK_STATUS(mock.create_event(other), Status::ok);
  CHECK(!mock.ring_doorbell(3u));
  mock.connect_doorbell(event, 3u);
  CHECK_EQ(mock.live_doorbells(), 1u);
  CHECK(mock.ring_doorbell(3u));
  mock.wait_event(event);
  CHECK(!mock.ring_doorbell(4u));
  CHECK(!mock.ring_doorbell(MockHost::max_doorbells));
  mock.connect_doorbell(other, 3u);
  mock.connect_doorbell(event, 5u);
  mock.connect_doorbell(other, MockHost::max_doorbells);
  int stranger = 0;
  mock.connect_doorbell(&stranger, 6u);
  mock.disconnect_doorbell(other, 3u);
  mock.destroy_event(event);
  for (int count = 0; count < 6; count++) CHECK(mock.claim(HostViolation::doorbell_misuse));
  CHECK_EQ(mock.live_events(), 2u);
  mock.disconnect_doorbell(event, 3u);
  CHECK_EQ(mock.live_doorbells(), 0u);
  mock.destroy_event(event);
  mock.destroy_event(other);
  void *thread = nullptr;
  CHECK_STATUS(mock.start_thread([](void *host) {
                   static_cast<MockHost *>(host)->disconnect_doorbell(nullptr, 1u);
               },
                                 &mock, thread),
               Status::ok);
  mock.join_thread(thread);
  CHECK(mock.claim(HostViolation::main_loop_required));
  CHECK(mock.claim(HostViolation::doorbell_misuse)); }

TEST(mock_host_fence_interrupts, counts_raises_while_connected_and_catches_misuse)
{ MockHost mock;
  CHECK(!mock.fence_interrupts_connected());
  mock.raise_fence_interrupt(0u);
  CHECK_EQ(mock.raised(0u), 0u);
  CHECK_STATUS(mock.connect_fence_interrupts(), Status::ok);
  CHECK(mock.fence_interrupts_connected());
  CHECK_STATUS(mock.connect_fence_interrupts(), Status::ok);
  mock.raise_fence_interrupt(3u);
  mock.raise_fence_interrupt(3u);
  mock.raise_fence_interrupt(MockHost::max_doorbells);
  mock.raise_fence_interrupt(MockHost::interrupt_lines);
  CHECK_EQ(mock.raised(3u), 2u);
  CHECK_EQ(mock.raised(4u), 0u);
  CHECK_EQ(mock.raised(MockHost::max_doorbells), 1u);
  CHECK_EQ(mock.raised(MockHost::interrupt_lines), 0u);
  mock.disconnect_fence_interrupts();
  mock.disconnect_fence_interrupts();
  for (int count = 0; count < 4; count++) CHECK(mock.claim(HostViolation::fence_misuse));
  mock.fail_fence_connect_at(1u);
  CHECK_STATUS(mock.connect_fence_interrupts(), Status::no_memory);
  CHECK(!mock.fence_interrupts_connected());
  void *thread = nullptr;
  CHECK_STATUS(mock.start_thread([](void *host) {
                   auto *const m = static_cast<MockHost *>(host);
                   m->disconnect_fence_interrupts();
                   static_cast<void>(m->connect_fence_interrupts());
               },
                                 &mock, thread),
               Status::ok);
  mock.join_thread(thread);
  CHECK(mock.claim(HostViolation::main_loop_required));
  CHECK(mock.claim(HostViolation::main_loop_required));
  CHECK(mock.claim(HostViolation::fence_misuse));
  mock.disconnect_fence_interrupts(); }

TEST(mock_host_alias_guest_run, a_second_mapping_shares_the_guest_pages_both_ways)
{ MockHost host;
  host.add_guest_ram(0x100000u, 0x4000u);
  std::uint64_t length = 0x2000u;
  bool          direct = false;
  void *const   run = host.map_guest(0x101000u, length, direct);
  CHECK(run != nullptr && direct && length == 0x2000u);
  void *base = nullptr;
  CHECK_STATUS(host.reserve_range(0x3000u, base), Status::ok);
  CHECK(base != nullptr);
  CHECK_EQ(host.live_ranges(), 1u);
  const auto *const at = static_cast<std::uint8_t *>(base) + 0x1000u;
  CHECK_STATUS(host.alias_guest_run(run, 0x2000u, static_cast<std::uint8_t *>(base) + 0x1000u), Status::ok);
  CHECK_EQ(host.live_aliases(), 1u);
  host.guest_ram(0x101FFFu)[0] = 0xC3u;
  CHECK_EQ(at[0xFFFu], 0xC3u);
  static_cast<std::uint8_t *>(base)[0x2ABCu] = 0x7Eu;
  CHECK_EQ(host.guest_ram(0x102ABCu)[0], 0x7Eu);
  host.release_range(base, 0x3000u);
  CHECK_EQ(host.live_ranges(), 0u);
  CHECK_EQ(host.live_aliases(), 0u);
  CHECK_EQ(host.guest_ram(0x102ABCu)[0], 0x7Eu);
  host.unmap_guest(run, length); }

TEST(mock_host_alias_guest_run, refuses_what_it_cannot_map_twice_and_catches_a_range_it_never_made)
{ MockHost host;
  host.add_guest_ram(0x100000u, 0x2000u);
  std::uint64_t length = 0x2000u;
  bool          direct = false;
  void *const   run = host.map_guest(0x100000u, length, direct);
  void *base = nullptr;
  CHECK_STATUS(host.reserve_range(0x4000u, base), Status::ok);
  std::uint8_t outside[0x1000] = {};
  CHECK_STATUS(host.alias_guest_run(outside, 0x1000u, base), Status::not_supported);
  CHECK_STATUS(host.alias_guest_run(run, 0x3000u, base), Status::not_supported);
  CHECK_STATUS(host.alias_guest_run(run, 0x800u, base), Status::not_supported);
  CHECK_STATUS(host.alias_guest_run(static_cast<std::uint8_t *>(run) + 1, 0x1000u, base), Status::not_supported);
  CHECK_STATUS(host.alias_guest_run(run, 0x1000u, static_cast<std::uint8_t *>(base) + 0x4000u),
               Status::invalid_parameter);
  CHECK(host.claim(HostViolation::unknown_handle));
  host.fail_alias_at(1u);
  CHECK_STATUS(host.alias_guest_run(run, 0x1000u, base), Status::not_supported);
  CHECK_STATUS(host.alias_guest_run(run, 0x1000u, base), Status::ok);
  CHECK_EQ(host.live_aliases(), 1u);
  host.release_range(base, 0x1000u);
  CHECK(host.claim(HostViolation::unknown_handle));
  host.release_range(base, 0x4000u);
  host.release_range(base, 0x4000u);
  CHECK(host.claim(HostViolation::unknown_handle));
  host.fail_reserve_at(2u);
  CHECK_STATUS(host.reserve_range(0x1000u, base), Status::ok);
  void *refused = &host;
  CHECK_STATUS(host.reserve_range(0x1000u, refused), Status::no_memory);
  CHECK(refused == nullptr);
  host.release_range(base, 0x1000u);
  CHECK_EQ(host.live_ranges(), 0u);
  host.unmap_guest(run, length); }
