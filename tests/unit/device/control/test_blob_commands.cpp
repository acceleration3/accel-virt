#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/control/blob_commands.hpp"
#include "device/recording/wire_recorder.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/host_blob_table.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/recording/recorded_file.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>
#include <cstring>

using av::BlobCommands;
using av::BlobTable;
using av::ControlRequest;
using av::GpuRequestBuilder;
using av::HostBlobId;
using av::HostBlobTable;
using av::HostMemoryRequest;
using av::MockHost;
using av::MockRequest;
using av::Status;
using av::WireRecorder;

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint32_t ok_nodata = 0x1100u, ok_map_info = 0x1106u, err_unspec = 0x1200u,
                        err_out_of_memory = 0x1201u, err_invalid_resource_id = 0x1203u,
                        err_invalid_context_id = 0x1204u, err_invalid_parameter = 0x1205u;
constexpr std::uint32_t guest = 1u, host3d = 2u, host3d_guest = 3u, mappable = 1u;
constexpr std::uint32_t map_cache_cached = 1u, map_cache_wc = 3u;
constexpr std::uint32_t context = 7u;
constexpr std::uint32_t host_blob = HostBlobId::first;
const std::uint64_t     upload = HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u).encoded();

GpuRequestBuilder create_blob(std::uint32_t id, std::uint32_t memory, std::uint32_t flags, std::uint32_t entries,
                              std::uint64_t blob_id, std::uint64_t size)
{
    GpuRequestBuilder request(BlobCommands::resource_create_blob);
    request.u32(id).u32(memory).u32(flags).u32(entries).u64(blob_id).u64(size);
    return request;
}

GpuRequestBuilder create_host(std::uint32_t id, std::uint64_t size, std::uint64_t memory = upload,
                              std::uint32_t for_context = context)
{
    return create_blob(id, host3d, mappable, 0u, memory, size).context(for_context);
}

const std::uint64_t readback = HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u).encoded();

GpuRequestBuilder create_adopted(std::uint32_t id, std::uint64_t size, std::uint64_t memory = readback)
{
    return create_blob(id, host3d_guest, 0u, 0u, memory, size).context(context);
}

GpuRequestBuilder attach(std::uint32_t id, std::uint64_t address, std::uint32_t length)
{
    GpuRequestBuilder request(BlobCommands::resource_attach_backing);
    request.u32(id).u32(1u).u64(address).u32(length).u32(0u);
    return request;
}

GpuRequestBuilder map_blob(std::uint32_t id, std::uint64_t offset)
{
    GpuRequestBuilder request(BlobCommands::resource_map_blob);
    request.u32(id).u32(0u).u64(offset);
    return request;
}

GpuRequestBuilder named(std::uint32_t type, std::uint32_t id)
{
    GpuRequestBuilder request(type);
    request.u32(id).u32(0u);
    return request;
}

class Fixture {
public:
    Fixture() : blobs_(host_), recorder_(host_), host_blobs_(host_), commands_(host_, blobs_, host_blobs_, recorder_, plane_)
    {
        static_cast<void>(blobs_.initialize());
        static_cast<void>(host_blobs_.initialize(plane_));
        plane_.hold_context(context);
        host_.add_guest_ram(ram, 0x10000u);
        host_.add_bounce_region(0x800000u, 0x1000u);
        host_.set_hostmem_size(0x10000u);
        host_.add_resource(50u);
    }
    ~Fixture() { host_blobs_.release_all(); }

    std::uint32_t send(const GpuRequestBuilder &builder, std::uint32_t *map_info = nullptr)
    {
        MockRequest mock(builder.bytes(), builder.size());
        ControlRequest request(host_, &mock);
        std::uint32_t type = 0u;
        std::memcpy(&type, builder.bytes(), sizeof type);
        if (!commands_.answer(request, type)) return mock.answered() ? 0xDEADu : 0u;
        if (map_info != nullptr) *map_info = mock.response_u32(24u);
        return mock.response_type();
    }
    std::uint32_t send_type(std::uint32_t type, const GpuRequestBuilder &builder)
    {
        MockRequest mock(builder.bytes(), builder.size());
        ControlRequest request(host_, &mock);
        return commands_.answer(request, type) ? mock.response_type() : 0u;
    }
    MockHost  &host() { return host_; }
    BlobTable &blobs() { return blobs_; }
    HostBlobTable &host_blobs() { return host_blobs_; }
    WireRecorder &recorder() { return recorder_; }
    av::RecordingContextPlane &plane() { return plane_; }

private:
    MockHost                  host_;
    BlobTable                 blobs_;
    WireRecorder              recorder_;
    av::RecordingContextPlane plane_;
    HostBlobTable             host_blobs_;
    BlobCommands              commands_;
};

}

TEST(blob_commands_create, a_guest_blob_over_its_pages)
{ Fixture fixture;
  GpuRequestBuilder request = create_blob(1u, guest, 0u, 2u, 0u, 0x3000u);
  request.u64(ram).u32(0x1000u).u32(0u).u64(ram + 0x4000u).u32(0x2000u).u32(0u);
  CHECK_EQ(fixture.send(request), ok_nodata);
  CHECK(fixture.blobs().contains(1u));
  CHECK(fixture.blobs().invariants_hold());
  CHECK_EQ(fixture.host().live_guest_mappings(), 2u); }

TEST(blob_commands_create, a_host_blob_is_memory_the_plane_allocates_for_its_context)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create_host(host_blob, 0x2000u)), ok_nodata);
  CHECK(fixture.host_blobs().contains(host_blob));
  CHECK(fixture.host_blobs().owns(context, host_blob));
  CHECK(!fixture.host_blobs().owns(context + 1u, host_blob));
  CHECK(fixture.host_blobs().invariants_hold());
  CHECK_EQ(fixture.plane().memory_allocations(), 1u);
  const av::RecordingContextPlane::MemoryAsked asked = fixture.plane().last_memory();
  CHECK_EQ(asked.context, context);
  CHECK_EQ(asked.key, std::uint64_t{host_blob});
  CHECK_EQ(asked.size, 0x2000u);
  CHECK_EQ(asked.request, static_cast<std::uint32_t>(upload));
  CHECK_EQ(fixture.blobs().live(), 0u);
  CHECK_EQ(fixture.plane().created_count(), 0u); }

TEST(blob_commands_create, an_adopted_blob_lies_over_the_pages_attached_to_it_has_no_descriptor_to_map_and_goes_on_release)
{ Fixture fixture;
  std::uint8_t *const first = fixture.host().guest_ram(ram + 0x3000u);
  first[5] = 0x5Au;
  CHECK_EQ(fixture.send(create_adopted(host_blob, 0x3000u)), ok_nodata);
  CHECK(fixture.host_blobs().owns(context, host_blob));
  CHECK_EQ(fixture.send(attach(host_blob, ram + 0x3000u, 0x1000u)), ok_nodata);
  CHECK_EQ(fixture.plane().last_memory().request, static_cast<std::uint32_t>(readback));
  CHECK_EQ(fixture.send(attach(host_blob, ram + 0x8000u, 0x2000u)), ok_nodata);
  CHECK_EQ(fixture.host().overlaid_bytes(), 0x3000u);
  CHECK_EQ(first[5], 0x5Au);
  CHECK_EQ(fixture.send(attach(host_blob, ram + 0xC000u, 0x1000u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(map_blob(host_blob, 0u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unmap_blob, host_blob)), err_invalid_parameter);
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unref, host_blob)), ok_nodata);
  CHECK_EQ(fixture.host().overlaid_bytes(), 0u);
  CHECK_EQ(first[5], 0x5Au);
  CHECK_EQ(fixture.send(create_adopted(host_blob, 0x1000u)), ok_nodata);
  CHECK_EQ(fixture.send(attach(host_blob, ram + 0x3000u, 0x1000u)), ok_nodata); }

TEST(blob_commands_create, an_adoption_the_protocol_or_the_memory_does_not_allow_holds_nothing)
{ struct Case {
      GpuRequestBuilder request;
      std::uint32_t     answer;
  };
  Case cases[] = {
      {create_blob(host_blob, host3d_guest, 0u, 1u, readback, 0x1000u).context(context), err_invalid_parameter},
      {create_blob(host_blob, host3d_guest, 0u, 0u, readback | std::uint64_t{1} << 32u, 0x1000u).context(context),
       err_invalid_parameter},
      {create_blob(host_blob, host3d_guest, 0u, 0u, readback, 0x1000u).context(context + 1u), err_invalid_context_id},
      {create_adopted(5u, 0x1000u), err_invalid_resource_id},
      {create_adopted(host_blob, 0x1800u), err_invalid_parameter},
  };
  for (Case &c : cases) {
      Fixture fixture;
      CHECK_EQ(fixture.send(c.request), c.answer);
      CHECK_EQ(fixture.host_blobs().live(), 0u);
  }
  { Fixture refused;
    refused.plane().refuse_memory_at(1u);
    CHECK_EQ(refused.send(create_adopted(host_blob, 0x1000u)), ok_nodata);
    CHECK_EQ(refused.send(attach(host_blob, ram + 0x3000u, 0x1000u)), err_out_of_memory); }
  Fixture combined;
  combined.plane().answer_caching(AV_MEMORY_WRITE_COMBINED);
  CHECK_EQ(combined.send(create_adopted(host_blob, 0x1000u)), ok_nodata);
  CHECK_EQ(combined.send(attach(host_blob, ram + 0x3000u, 0x1000u)), err_unspec);
  CHECK_EQ(combined.plane().freed_count(), 1u); }

TEST(blob_commands_attach, a_piece_is_laid_or_refused_and_anything_not_a_host_blob_is_the_base_class_s)
{ Fixture fixture;
  CHECK_EQ(fixture.send(attach(50u, ram, 0x1000u)), 0u);
  CHECK_EQ(fixture.send(attach(host_blob, ram, 0x1000u)), 0u);
  CHECK_EQ(fixture.send(attach(host_blob, ram, 0x1000u).truncate(31u)), 0u);
  CHECK_EQ(fixture.send(create_adopted(host_blob, 0x4000u)), ok_nodata);
  CHECK_EQ(fixture.send(create_adopted(host_blob + 1u, 0x4000u)), ok_nodata);
  CHECK_EQ(fixture.send(attach(host_blob, ram, 0x1000u)), ok_nodata);
  CHECK_EQ(fixture.send(attach(host_blob + 1u, ram, 0x1000u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(attach(host_blob, 0x800000u, 0x1000u)), err_unspec);
  fixture.host().fail_allocation_at(1u);
  CHECK_EQ(fixture.send(attach(host_blob + 1u, ram + 0x1000u, 0x1000u)), err_out_of_memory);
  CHECK_EQ(fixture.host().overlaid_bytes(), 0x1000u); }

TEST(blob_commands_map, a_host_blob_maps_where_the_guest_asks_with_the_host_s_caching)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create_host(host_blob, 0x2000u)), ok_nodata);
  std::uint32_t map_info = 0u;
  CHECK_EQ(fixture.send(map_blob(host_blob, 0x4000u), &map_info), ok_map_info);
  CHECK_EQ(map_info, map_cache_cached);
  CHECK_EQ(fixture.host().last_placement().offset, 0x4000u);
  CHECK_EQ(fixture.host().last_placement().size, 0x2000u);
  auto *const placed = static_cast<std::uint8_t *>(fixture.host().placed_at(0x4000u));
  CHECK(placed != nullptr);
  CHECK(fixture.host_blobs().invariants_hold());
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unmap_blob, host_blob)), ok_nodata);
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unmap_blob, host_blob)), err_invalid_parameter);
  fixture.plane().answer_caching(AV_MEMORY_WRITE_COMBINED);
  CHECK_EQ(fixture.send(create_host(host_blob + 1u, 0x1000u)), ok_nodata);
  CHECK_EQ(fixture.send(map_blob(host_blob + 1u, 0x8000u), &map_info), ok_map_info);
  CHECK_EQ(map_info, map_cache_wc); }

TEST(blob_commands_create, refuses_what_the_protocol_does_not_allow)
{ struct Case {
      GpuRequestBuilder request;
      std::uint32_t     answer;
  };
  Case cases[] = {
      {create_blob(1u, guest, 0u, 1u, 0u, 0x1000u).truncate(55u), err_invalid_parameter},
      {create_host(0u, 0x1000u), err_invalid_resource_id},
      {create_host(50u, 0x1000u), err_invalid_resource_id},
      {create_host(5u, 0x1000u), err_invalid_resource_id},
      {create_host(HostBlobId::first - 1u, 0x1000u), err_invalid_resource_id},
      {create_blob(1u, guest, 0u, 1u, 7u, 0x1000u), err_invalid_parameter},
      {create_blob(host_blob, host3d_guest, mappable, 0u, 0u, 0x1000u).context(context), err_invalid_parameter},
      {create_blob(host_blob, host3d, 0u, 0u, upload, 0x1000u).context(context), err_invalid_parameter},
      {create_blob(host_blob, host3d, mappable, 1u, upload, 0x1000u).context(context), err_invalid_parameter},
      {create_host(host_blob, 0x1000u, upload | std::uint64_t{1} << 32u), err_invalid_parameter},
      {create_host(host_blob, 0x1000u, upload, context + 1u), err_invalid_context_id},
      {create_host(host_blob, 0x1000u, upload, 0u), err_invalid_context_id},
      {create_host(host_blob, 0x1800u), err_invalid_parameter},
      {create_host(host_blob, 0u), err_invalid_parameter},
      {create_host(host_blob, 0x1000u, 0u), err_invalid_parameter},
      {create_host(host_blob, 0x1000u, 9u), err_invalid_parameter},
      {create_host(host_blob, 0x1000u, 2u | 1u << 8u), err_invalid_parameter},
      {create_blob(1u, guest, 0u, 0u, 0u, 0x1000u), err_invalid_parameter},
      {create_blob(1u, guest, 0u, 2u, 0u, 0x1000u), err_invalid_parameter},
  };
  for (Case &c : cases) {
      Fixture fixture;
      CHECK_EQ(fixture.send(c.request), c.answer);
      CHECK_EQ(fixture.blobs().live(), 0u);
      CHECK_EQ(fixture.host_blobs().live(), 0u);
      CHECK_EQ(fixture.plane().memory_allocations(), 0u);
  }
  Fixture short_backing;
  GpuRequestBuilder small = create_blob(1u, guest, 0u, 1u, 0u, 0x2000u);
  small.u64(ram).u32(0x1000u).u32(0u);
  CHECK_EQ(short_backing.send(small), err_invalid_parameter);
  CHECK_EQ(short_backing.host().live_guest_mappings(), 0u);
  Fixture bounce;
  GpuRequestBuilder device_memory = create_blob(1u, guest, 0u, 1u, 0u, 0x1000u);
  device_memory.u64(0x800000u).u32(0x1000u).u32(0u);
  CHECK_EQ(bounce.send(device_memory), err_unspec);
  Fixture duplicate;
  CHECK_EQ(duplicate.send(create_host(host_blob, 0x1000u)), ok_nodata);
  CHECK_EQ(duplicate.send(create_host(host_blob, 0x1000u)), err_invalid_resource_id);
  CHECK_EQ(duplicate.plane().memory_allocations(), 1u);
  Fixture taken;
  GpuRequestBuilder guest_there = create_blob(host_blob, guest, 0u, 1u, 0u, 0x1000u);
  guest_there.u64(ram).u32(0x1000u).u32(0u);
  CHECK_EQ(taken.send(guest_there), ok_nodata);
  CHECK_EQ(taken.send(create_host(host_blob, 0x1000u)), err_invalid_resource_id);
  CHECK_EQ(taken.host_blobs().live(), 0u); }

TEST(blob_commands_create, memory_the_plane_cannot_give_is_out_of_memory_and_holds_nothing)
{ { Fixture refused;
    refused.plane().refuse_memory_at(1u);
    CHECK_EQ(refused.send(create_host(host_blob, 0x1000u)), err_out_of_memory);
    CHECK(!refused.host_blobs().contains(host_blob)); }
  { Fixture short_answer;
    short_answer.plane().answer_short();
    CHECK_EQ(short_answer.send(create_host(host_blob, 0x2000u)), err_out_of_memory);
    CHECK_EQ(short_answer.plane().freed_count(), 1u);
    CHECK_EQ(short_answer.plane().freed(0u), std::uint64_t{host_blob}); }
  { Fixture no_descriptor;
    no_descriptor.plane().answer_without_descriptor();
    CHECK_EQ(no_descriptor.send(create_host(host_blob, 0x1000u)), err_out_of_memory);
    CHECK_EQ(no_descriptor.plane().freed_count(), 1u); }
  const std::uint32_t uncachable_answers[] = {0u, 3u};
  for (const std::uint32_t caching : uncachable_answers) {
      Fixture uncachable;
      uncachable.plane().answer_caching(caching);
      CHECK_EQ(uncachable.send(create_host(host_blob, 0x1000u)), err_out_of_memory);
      CHECK_EQ(uncachable.plane().freed_count(), 1u);
      CHECK_EQ(uncachable.host_blobs().live(), 0u);
  } }

TEST(blob_commands_create, a_host_blob_table_never_initialized_refuses_every_host_blob)
{ MockHost                  host;
  BlobTable                 blobs(host);
  WireRecorder              recorder(host);
  av::RecordingContextPlane plane;
  HostBlobTable             host_blobs(host);
  BlobCommands              commands(host, blobs, host_blobs, recorder, plane);
  plane.hold_context(context);
  const GpuRequestBuilder builder = create_host(host_blob, 0x1000u);
  MockRequest             mock(builder.bytes(), builder.size());
  ControlRequest          request(host, &mock);
  CHECK(commands.answer(request, BlobCommands::resource_create_blob));
  CHECK_EQ(mock.response_type(), err_invalid_parameter);
  CHECK_EQ(plane.memory_allocations(), 0u); }

TEST(blob_commands_create, a_guest_blob_the_host_has_no_memory_to_track_is_out_of_memory)
{ Fixture fixture;
  fixture.host().fail_allocation_at(1u);
  GpuRequestBuilder request = create_blob(1u, guest, 0u, 1u, 0u, 0x1000u);
  request.u64(ram).u32(0x1000u).u32(0u);
  CHECK_EQ(fixture.send(request), err_out_of_memory);
  CHECK(!fixture.blobs().contains(1u));
  CHECK_EQ(fixture.plane().created_count(), 0u);
  CHECK_EQ(fixture.send(request), ok_nodata); }

TEST(blob_commands_map, refuses_bad_offsets_second_maps_and_unmappable_blobs)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create_host(host_blob, 0x2000u)), ok_nodata);
  CHECK_EQ(fixture.send(create_host(host_blob + 1u, 0x2000u)), ok_nodata);
  GpuRequestBuilder guest_blob = create_blob(4u, guest, 0u, 1u, 0u, 0x1000u);
  guest_blob.u64(ram).u32(0x1000u).u32(0u);
  CHECK_EQ(fixture.send(guest_blob), ok_nodata);
  const std::uint32_t second = host_blob + 1u;
  CHECK_EQ(fixture.send(map_blob(host_blob, 0x2000u)), ok_map_info);
  CHECK_EQ(fixture.send(map_blob(host_blob, 0x8000u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(map_blob(second, 0x3000u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(map_blob(second, 0x1000u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(map_blob(second, 0x0800u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(map_blob(second, 0xF000u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(map_blob(second, ~std::uint64_t{0} - 0xFFFu)), err_invalid_parameter);
  CHECK_EQ(fixture.send(map_blob(4u, 0x8000u)), err_invalid_resource_id);
  CHECK_EQ(fixture.send(map_blob(9u, 0x8000u)), err_invalid_resource_id);
  CHECK_EQ(fixture.send(map_blob(second, 0x8000u).truncate(39u)), err_invalid_parameter);
  CHECK_EQ(fixture.host().live_placements(), 1u);
  fixture.host().fail_placement_at(1u);
  CHECK_EQ(fixture.send(map_blob(second, 0x8000u)), err_unspec);
  CHECK(fixture.host_blobs().invariants_hold());
  CHECK_EQ(fixture.send(map_blob(second, 0x4000u)), ok_map_info);
  CHECK_EQ(fixture.host().live_placements(), 2u);
  CHECK(fixture.host_blobs().invariants_hold()); }

TEST(blob_commands_unmap, refuses_unknown_and_short_requests)
{ Fixture fixture;
  GpuRequestBuilder unknown(BlobCommands::resource_unmap_blob);
  unknown.u32(8u).u32(0u);
  CHECK_EQ(fixture.send(unknown), err_invalid_resource_id);
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unmap_blob, host_blob)), err_invalid_resource_id);
  CHECK_EQ(fixture.send(GpuRequestBuilder(BlobCommands::resource_unmap_blob).u32(8u)), err_invalid_parameter); }

TEST(blob_commands_unref, releases_a_host_blob_mapped_or_not_and_leaves_other_resources_to_the_base_class)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create_host(host_blob, 0x1000u)), ok_nodata);
  CHECK_EQ(fixture.send(create_host(host_blob + 1u, 0x1000u)), ok_nodata);
  CHECK_EQ(fixture.send(map_blob(host_blob, 0u)), ok_map_info);
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unref, host_blob)), ok_nodata);
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unref, host_blob + 1u)), ok_nodata);
  CHECK_EQ(fixture.host_blobs().live(), 0u);
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK_EQ(fixture.plane().freed_count(), 2u);
  CHECK_EQ(fixture.plane().freed(0u), std::uint64_t{host_blob});
  CHECK_EQ(fixture.plane().freed(1u), std::uint64_t{host_blob + 1u});
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unref, host_blob)), 0u);
  CHECK_EQ(fixture.send(GpuRequestBuilder(BlobCommands::resource_unref).u32(host_blob)), 0u); }

TEST(blob_commands_unref, a_blob_a_context_is_bound_to_is_refused_and_kept)
{ Fixture fixture;
  GpuRequestBuilder request = create_blob(2u, guest, 0u, 1u, 0u, 0x1000u);
  request.u64(ram).u32(0x1000u).u32(0u);
  CHECK_EQ(fixture.send(request), ok_nodata);
  CHECK_STATUS(fixture.blobs().pin(2u, 0u), Status::ok);
  GpuRequestBuilder unref(BlobCommands::resource_unref);
  unref.u32(2u).u32(0u);
  CHECK_EQ(fixture.send(unref), err_invalid_parameter);
  CHECK(fixture.blobs().contains(2u));
  fixture.blobs().unpin(2u);
  CHECK_EQ(fixture.send(unref), ok_nodata); }

TEST(blob_commands, create_2d_on_a_blob_id_is_refused_and_everything_else_is_the_base_class_s)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create_host(host_blob, 0x1000u)), ok_nodata);
  GpuRequestBuilder guest_blob = create_blob(2u, guest, 0u, 1u, 0u, 0x1000u);
  guest_blob.u64(ram).u32(0x1000u).u32(0u);
  CHECK_EQ(fixture.send(guest_blob), ok_nodata);
  GpuRequestBuilder create_2d(BlobCommands::resource_create_2d);
  create_2d.u32(2u).u32(1u).u32(64u).u32(64u);
  CHECK_EQ(fixture.send(create_2d), err_invalid_resource_id);
  GpuRequestBuilder host_2d(BlobCommands::resource_create_2d);
  host_2d.u32(host_blob).u32(1u).u32(64u).u32(64u);
  CHECK_EQ(fixture.send(host_2d), err_invalid_resource_id);
  GpuRequestBuilder other_2d(BlobCommands::resource_create_2d);
  other_2d.u32(3u).u32(1u).u32(64u).u32(64u);
  CHECK_EQ(fixture.send(other_2d), 0u);
  CHECK_EQ(fixture.send_type(0x010Du, GpuRequestBuilder(0x010Du)), 0u);
  CHECK_EQ(fixture.send_type(0x0100u, GpuRequestBuilder(0x0100u)), 0u); }

std::uint32_t u32_at(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

TEST(blob_commands_create, records_every_blob_created_and_released)
{ Fixture fixture;
  CHECK_STATUS(fixture.recorder().open("blobs.av", 0u, "blob", av::AtCeiling::stop, av::GpuIdentity(),
                                       av::ProtocolVersion(1u, 1u)),
               Status::ok);
  GpuRequestBuilder request = create_blob(1u, guest, 0u, 2u, 0u, 0x3000u);
  request.u64(ram).u32(0x1000u).u32(0u).u64(ram + 0x4000u).u32(0x2000u).u32(0u);
  CHECK_EQ(fixture.send(request), ok_nodata);
  CHECK_EQ(fixture.send(create_host(host_blob, 0x2000u)), ok_nodata);
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unref, 1u)), ok_nodata);
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unref, host_blob)), ok_nodata);

  struct Expected {
      std::uint8_t  action;
      std::uint32_t context;
      std::uint32_t resource;
      std::uint64_t size;
  };
  const Expected expected[] = {{0u, 0u, 1u, 0x3000u},
                               {0u, context, host_blob, 0x2000u},
                               {2u, 0u, 1u, 0u},
                               {2u, 0u, host_blob, 0u}};
  av::RecordedFile file(fixture.host(), "blobs.av");
  for (const Expected &blob : expected) {
      av::RecordHeader    record;
      const std::uint8_t *payload = file.expect(av::RecordKind::blob, record);
      CHECK(payload != nullptr);
      if (payload == nullptr) break;
      CHECK_EQ(payload[0], blob.action);
      CHECK_EQ(record.context(), blob.context);
      CHECK_EQ(u32_at(payload, 8u), blob.resource);
      CHECK_EQ(u64_at(payload, 24u), blob.size);
  }
  CHECK_EQ(file.remaining(), 0u);
  fixture.recorder().close(); }

TEST(blob_commands, tells_the_contexts_of_every_guest_blob_made_and_let_go)
{ Fixture fixture;
  GpuRequestBuilder request = create_blob(1u, guest, 0u, 1u, 0u, 0x1000u);
  request.u64(ram).u32(0x1000u).u32(0u);
  CHECK_EQ(fixture.send(request), ok_nodata);
  GpuRequestBuilder second = create_blob(2u, guest, 0u, 1u, 0u, 0x1000u);
  second.u64(ram + 0x1000u).u32(0x1000u).u32(0u);
  CHECK_EQ(fixture.send(second), ok_nodata);
  CHECK_EQ(fixture.send(create_blob(3u, guest, 0u, 0u, 0u, 0x1000u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(create_host(host_blob, 0x1000u)), ok_nodata);
  CHECK_EQ(fixture.plane().created_count(), 2u);
  CHECK_EQ(fixture.plane().created(0u), 1u);
  CHECK_EQ(fixture.plane().created(1u), 2u);
  CHECK_EQ(fixture.plane().released_count(), 0u);
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unref, 2u)), ok_nodata);
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unref, host_blob)), ok_nodata);
  CHECK_EQ(fixture.plane().released_count(), 1u);
  CHECK_EQ(fixture.plane().released(0u), 2u);
  CHECK_EQ(fixture.send(named(BlobCommands::resource_unref, 9u)), 0u);
  CHECK_EQ(fixture.plane().released_count(), 1u); }

TEST(blob_commands_create, a_creation_the_device_refused_records_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.recorder().open("refused.av", 0u, "blob", av::AtCeiling::stop, av::GpuIdentity(),
                                       av::ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK_EQ(fixture.send(create_blob(host_blob, host3d, 0u, 0u, upload, 0x2000u).context(context)),
           err_invalid_parameter);
  fixture.plane().refuse_memory_at(1u);
  CHECK_EQ(fixture.send(create_host(host_blob, 0x2000u)), err_out_of_memory);
  static_cast<void>(fixture.send(named(BlobCommands::resource_unref, 99u)));
  CHECK_EQ(av::RecordedFile(fixture.host(), "refused.av").remaining(), 0u);
  fixture.recorder().close(); }
