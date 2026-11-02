#include "core/support/status.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/blob_resource.hpp"
#include "device/memory/blob_window.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>

using av::BlobResource;
using av::ControlRequest;
using av::GpuRequestBuilder;
using av::HostViolation;
using av::MockHost;
using av::MockRequest;
using av::Status;

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::size_t   entries_at = 24u;

GpuRequestBuilder entries(std::uint32_t pages)
{
    GpuRequestBuilder request(0u);
    for (std::uint32_t page = 0u; page < pages; page++) request.u64(ram + page * 0x2000u).u32(0x1000u).u32(0u);
    return request;
}

}

TEST(blob_resource_back_with_guest, holds_the_pages_that_cover_its_size)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  const GpuRequestBuilder bytes = entries(2u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  { BlobResource blob(host, 7u);
    CHECK_EQ(blob.id(), 7u);
    CHECK_EQ(blob.size(), 0u);
    CHECK_STATUS(blob.back_with_guest(request, entries_at, 2u, 0x1800u), Status::ok);
    CHECK_EQ(blob.size(), 0x2000u);
    CHECK_EQ(host.live_guest_mappings(), 2u); }
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(blob_resource_back_with_guest, exactly_its_size_is_enough)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  const GpuRequestBuilder bytes = entries(1u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  BlobResource blob(host, 1u);
  CHECK_STATUS(blob.back_with_guest(request, entries_at, 1u, 0x1000u), Status::ok);
  CHECK_EQ(blob.size(), 0x1000u); }

TEST(blob_resource_back_with_guest, pages_short_of_its_size_or_a_mapping_failure_leave_nothing_held)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  const GpuRequestBuilder bytes = entries(1u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  BlobResource short_blob(host, 1u);
  CHECK_STATUS(short_blob.back_with_guest(request, entries_at, 1u, 0x1001u), Status::invalid_parameter);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK_EQ(short_blob.size(), 0u);
  CHECK_EQ(short_blob.window().length(), 0u);
  BlobResource unmappable(host, 2u);
  CHECK(unmappable.back_with_guest(request, entries_at, 2u, 0x1000u) != Status::ok);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK_EQ(unmappable.size(), 0u); }

TEST(blob_resource_window, is_its_guest_pages)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  GpuRequestBuilder request_bytes = entries(1u);
  MockRequest mock(request_bytes.bytes(), request_bytes.size());
  ControlRequest request(host, &mock);
  BlobResource guest(host, 1u);
  CHECK_EQ(guest.window().length(), 0u);
  CHECK_STATUS(guest.back_with_guest(request, entries_at, 1u, 0x1000u), Status::ok);
  CHECK_EQ(guest.window().length(), 0x1000u);
  const std::uint8_t byte = 0x77u;
  CHECK_STATUS(guest.window().write(5u, &byte, 1u), Status::ok);
  CHECK_EQ(host.guest_ram(ram + 5u)[0], 0x77u); }

TEST(blob_resource_pin, counts_pins_until_the_last_is_let_go)
{ MockHost host;
  BlobResource blob(host, 1u);
  CHECK(!blob.pinned());
  blob.pin();
  blob.pin();
  CHECK(blob.pinned());
  blob.unpin();
  CHECK(blob.pinned());
  blob.unpin();
  CHECK(!blob.pinned()); }

TEST(blob_resource_pin, letting_go_of_a_pin_never_taken_leaves_it_unpinned)
{ MockHost host;
  BlobResource blob(host, 1u);
  blob.unpin();
  CHECK(!blob.pinned());
  blob.pin();
  CHECK(blob.pinned());
  blob.unpin();
  CHECK(!blob.pinned()); }

TEST(blob_resource_view, is_one_range_over_every_page_made_once)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  const GpuRequestBuilder bytes = entries(2u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  { BlobResource blob(host, 1u);
    CHECK_STATUS(blob.back_with_guest(request, entries_at, 2u, 0x2000u), Status::ok);
    host.guest_ram(ram + 0x10u)[0] = 0x11u;
    host.guest_ram(ram + 0x2010u)[0] = 0x22u;
    const std::uint8_t *base = nullptr;
    CHECK_STATUS(blob.view(base), Status::ok);
    CHECK(base != nullptr);
    CHECK(base != host.guest_ram(ram));
    CHECK_EQ(base[0x10], 0x11u);
    CHECK_EQ(base[0x1010], 0x22u);
    host.guest_ram(ram + 0x2010u)[0] = 0x33u;
    CHECK_EQ(base[0x1010], 0x33u);
    const std::uint8_t *again = nullptr;
    CHECK_STATUS(blob.view(again), Status::ok);
    CHECK(again == base);
    CHECK_EQ(host.live_ranges(), 1u); }
  CHECK_EQ(host.live_ranges(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(blob_resource_view, a_range_the_host_refuses_is_answered_and_asked_again_next_time)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  const GpuRequestBuilder bytes = entries(1u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  BlobResource blob(host, 1u);
  CHECK_STATUS(blob.back_with_guest(request, entries_at, 1u, 0x1000u), Status::ok);
  host.fail_reserve_at(1u);
  const std::uint8_t *base = &bytes.bytes()[0];
  CHECK(blob.view(base) != Status::ok);
  CHECK(base == nullptr);
  CHECK_EQ(host.live_ranges(), 0u);
  CHECK_STATUS(blob.view(base), Status::ok);
  CHECK(base != nullptr); }

TEST(blob_resource_view, a_blob_with_no_pages_has_no_view)
{ MockHost host;
  BlobResource blob(host, 1u);
  const std::uint8_t *base = nullptr;
  CHECK(blob.view(base) != Status::ok);
  CHECK(base == nullptr);
  CHECK_EQ(host.live_ranges(), 0u); }

TEST(blob_resource, is_main_loop_only)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  const GpuRequestBuilder bytes = entries(1u);
  MockRequest mock(bytes.bytes(), bytes.size());
  struct Call {
      MockHost    *host;
      MockRequest *mock;
      Status       backed;
  } call = {&host, &mock, Status::ok};
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const   c = static_cast<Call *>(context);
                       ControlRequest request(*c->host, c->mock);
                       BlobResource  blob(*c->host, 1u);
                       c->backed = blob.back_with_guest(request, entries_at, 0u, 0x1000u);
                       const std::uint8_t *base = nullptr;
                       static_cast<void>(blob.view(base));
                   },
                   &call, thread),
               Status::ok);
  host.join_thread(thread);
  CHECK(host.claim(HostViolation::main_loop_required));
  CHECK(host.claim(HostViolation::main_loop_required));
  while (host.claim(HostViolation::main_loop_required)) {}
  CHECK(call.backed != Status::ok); }
