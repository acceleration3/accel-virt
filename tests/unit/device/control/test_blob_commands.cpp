/* The blob commands the core answers, request byte by request byte: every
 * answer, every refusal, and what the base class is left to answer. */
#include "core/support/status.hpp"
#include "device/control/blob_commands.hpp"
#include "device/recording/wire_recorder.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/guest_page_alias.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/recording/recorded_file.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>
#include <cstring>

using av::BlobCommands;
using av::BlobTable;
using av::ControlRequest;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockRequest;
using av::Status;
using av::WireRecorder;

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint32_t ok_nodata = 0x1100u, ok_map_info = 0x1106u, err_unspec = 0x1200u,
                        err_out_of_memory = 0x1201u, err_invalid_resource_id = 0x1203u,
                        err_invalid_parameter = 0x1205u;
constexpr std::uint32_t guest = 1u, host3d = 2u, host3d_guest = 3u, mappable = 1u;

GpuRequestBuilder create_blob(std::uint32_t id, std::uint32_t memory, std::uint32_t flags, std::uint32_t entries,
                              std::uint64_t blob_id, std::uint64_t size)
{
    GpuRequestBuilder request(BlobCommands::resource_create_blob);
    request.u32(id).u32(memory).u32(flags).u32(entries).u64(blob_id).u64(size);
    return request;
}

class Fixture {
public:
    Fixture() : blobs_(host_), recorder_(host_), commands_(host_, blobs_, recorder_)
    {
        static_cast<void>(blobs_.initialize());
        host_.add_guest_ram(ram, 0x10000u);
        host_.add_bounce_region(0x800000u, 0x1000u);
        host_.set_hostmem_size(0x10000u);
        host_.add_resource(50u);
    }

    /* The response type, or 0 when the core left the request to the base class. */
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
    WireRecorder &recorder() { return recorder_; }

private:
    MockHost     host_;
    BlobTable    blobs_;
    WireRecorder recorder_;
    BlobCommands commands_;
};

}  // namespace

TEST(blob_commands_create, a_guest_blob_over_its_pages)
{ Fixture fixture;
  GpuRequestBuilder request = create_blob(1u, guest, 0u, 2u, 0u, 0x3000u);
  request.u64(ram).u32(0x1000u).u32(0u).u64(ram + 0x4000u).u32(0x2000u).u32(0u);
  CHECK_EQ(fixture.send(request), ok_nodata);
  CHECK(fixture.blobs().contains(1u));
  CHECK(fixture.blobs().invariants_hold());
  CHECK_EQ(fixture.host().live_guest_mappings(), 2u); }

TEST(blob_commands_create, a_host_visible_blob_maps_and_unmaps)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create_blob(2u, host3d, mappable, 0u, 0u, 0x2000u)), ok_nodata);
  std::uint32_t map_info = 0u;
  GpuRequestBuilder map(BlobCommands::resource_map_blob);
  map.u32(2u).u32(0u).u64(0x4000u);
  CHECK_EQ(fixture.send(map, &map_info), ok_map_info);
  CHECK_EQ(map_info, 1u);   /* VIRTIO_GPU_MAP_CACHE_CACHED */
  CHECK(fixture.host().placed_at(0x4000u) != nullptr);
  CHECK(fixture.blobs().invariants_hold());
  GpuRequestBuilder unmap(BlobCommands::resource_unmap_blob);
  unmap.u32(2u).u32(0u);
  CHECK_EQ(fixture.send(unmap), ok_nodata);
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK_EQ(fixture.send(unmap), err_invalid_parameter); }

TEST(blob_commands_create, refuses_what_the_protocol_does_not_allow)
{ struct Case {
      GpuRequestBuilder request;
      std::uint32_t     answer;
  };
  Case cases[] = {
      {create_blob(1u, guest, 0u, 1u, 0u, 0x1000u).truncate(55u), err_invalid_parameter},
      {create_blob(0u, host3d, mappable, 0u, 0u, 0x1000u), err_invalid_resource_id},
      {create_blob(50u, host3d, mappable, 0u, 0u, 0x1000u), err_invalid_resource_id},   /* a base-class 2D resource */
      {create_blob(1u, host3d, mappable, 0u, 7u, 0x1000u), err_invalid_parameter},     /* names a context object */
      {create_blob(1u, host3d_guest, mappable, 0u, 0u, 0x1000u), err_invalid_parameter},
      {create_blob(1u, host3d, 0u, 0u, 0u, 0x1000u), err_invalid_parameter},          /* not mappable */
      {create_blob(1u, host3d, mappable, 1u, 0u, 0x1000u), err_invalid_parameter},    /* entries for host memory */
      {create_blob(1u, host3d, mappable, 0u, 0u, 0x1800u), err_invalid_parameter},    /* not a page multiple */
      {create_blob(1u, guest, 0u, 0u, 0u, 0x1000u), err_invalid_parameter},           /* no entries */
      {create_blob(1u, guest, 0u, 2u, 0u, 0x1000u), err_invalid_parameter},           /* entries missing */
  };
  for (Case &c : cases) {
      Fixture fixture;
      CHECK_EQ(fixture.send(c.request), c.answer);
      CHECK_EQ(fixture.blobs().live(), 0u);
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
  Fixture exhausted;
  exhausted.host().fail_host_memory_at(1u);
  CHECK_EQ(exhausted.send(create_blob(1u, host3d, mappable, 0u, 0u, 0x1000u)), err_out_of_memory);
  Fixture duplicate;
  CHECK_EQ(duplicate.send(create_blob(3u, host3d, mappable, 0u, 0u, 0x1000u)), ok_nodata);
  CHECK_EQ(duplicate.send(create_blob(3u, host3d, mappable, 0u, 0u, 0x1000u)), err_invalid_resource_id); }

TEST(blob_commands_create, a_full_table_is_out_of_memory)
{ Fixture fixture;
  for (std::uint32_t id = 100u; id < 100u + BlobTable::max_blobs; id++) {
      if (fixture.send(create_blob(id, guest, 0u, 1u, 0u, 0x10u).u64(ram).u32(0x10u).u32(0u)) != ok_nodata) {
          CHECK(false);
          break;
      }
  }
  GpuRequestBuilder one_more = create_blob(5000u, guest, 0u, 1u, 0u, 0x10u);
  one_more.u64(ram).u32(0x10u).u32(0u);
  CHECK_EQ(fixture.send(one_more), err_out_of_memory);
  fixture.blobs().release_all(); }

TEST(blob_commands_map, refuses_bad_offsets_second_maps_and_unmappable_blobs)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create_blob(2u, host3d, mappable, 0u, 0u, 0x2000u)), ok_nodata);
  CHECK_EQ(fixture.send(create_blob(3u, host3d, mappable, 0u, 0u, 0x2000u)), ok_nodata);
  GpuRequestBuilder guest_blob = create_blob(4u, guest, 0u, 1u, 0u, 0x1000u);
  guest_blob.u64(ram).u32(0x1000u).u32(0u);
  CHECK_EQ(fixture.send(guest_blob), ok_nodata);
  const auto map = [](std::uint32_t id, std::uint64_t offset) {
      GpuRequestBuilder request(BlobCommands::resource_map_blob);
      request.u32(id).u32(0u).u64(offset);
      return request;
  };
  CHECK_EQ(fixture.send(map(2u, 0x2000u)), ok_map_info);
  CHECK_EQ(fixture.send(map(2u, 0x8000u)), err_invalid_parameter);   /* already mapped */
  CHECK_EQ(fixture.send(map(3u, 0x3000u)), err_invalid_parameter);   /* overlaps */
  CHECK_EQ(fixture.send(map(3u, 0x0800u)), err_invalid_parameter);   /* misaligned */
  CHECK_EQ(fixture.send(map(3u, 0xF000u)), err_invalid_parameter);   /* past the region */
  CHECK_EQ(fixture.send(map(4u, 0x8000u)), err_invalid_parameter);   /* guest blob */
  CHECK_EQ(fixture.send(map(9u, 0x8000u)), err_invalid_resource_id);
  CHECK_EQ(fixture.send(map(3u, 0x8000u).truncate(39u)), err_invalid_parameter);
  fixture.host().fail_placement_at(1u);
  CHECK_EQ(fixture.send(map(3u, 0x8000u)), err_unspec);
  CHECK(fixture.blobs().invariants_hold());
  CHECK_EQ(fixture.send(map(3u, 0x4000u)), ok_map_info); }

TEST(blob_commands_unmap, refuses_unknown_and_short_requests)
{ Fixture fixture;
  GpuRequestBuilder unknown(BlobCommands::resource_unmap_blob);
  unknown.u32(8u).u32(0u);
  CHECK_EQ(fixture.send(unknown), err_invalid_resource_id);
  CHECK_EQ(fixture.send(GpuRequestBuilder(BlobCommands::resource_unmap_blob).u32(8u)), err_invalid_parameter); }

TEST(blob_commands_unref, releases_a_blob_mapped_or_not_and_leaves_other_resources_to_the_base_class)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create_blob(2u, host3d, mappable, 0u, 0u, 0x1000u)), ok_nodata);
  GpuRequestBuilder map(BlobCommands::resource_map_blob);
  map.u32(2u).u32(0u).u64(0u);
  CHECK_EQ(fixture.send(map), ok_map_info);
  GpuRequestBuilder unref(BlobCommands::resource_unref);
  unref.u32(2u).u32(0u);
  CHECK_EQ(fixture.send(unref), ok_nodata);
  CHECK_EQ(fixture.blobs().live(), 0u);
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK_EQ(fixture.host().live_host_memory(), 0u);
  CHECK_EQ(fixture.send(unref), 0u);                                          /* base class answers */
  CHECK_EQ(fixture.send(GpuRequestBuilder(BlobCommands::resource_unref).u32(2u)), 0u); }

TEST(blob_commands_unref, a_blob_a_context_is_bound_to_is_refused_and_kept)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create_blob(2u, host3d, mappable, 0u, 0u, 0x1000u)), ok_nodata);
  CHECK_STATUS(fixture.blobs().pin(2u, 0u), Status::ok);
  GpuRequestBuilder unref(BlobCommands::resource_unref);
  unref.u32(2u).u32(0u);
  CHECK_EQ(fixture.send(unref), err_invalid_parameter);
  CHECK(fixture.blobs().contains(2u));
  fixture.blobs().unpin(2u);
  CHECK_EQ(fixture.send(unref), ok_nodata); }

TEST(blob_commands, create_2d_on_a_blob_id_is_refused_and_everything_else_is_the_base_class_s)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create_blob(2u, host3d, mappable, 0u, 0u, 0x1000u)), ok_nodata);
  GpuRequestBuilder create_2d(BlobCommands::resource_create_2d);
  create_2d.u32(2u).u32(1u).u32(64u).u32(64u);
  CHECK_EQ(fixture.send(create_2d), err_invalid_resource_id);
  GpuRequestBuilder other_2d(BlobCommands::resource_create_2d);
  other_2d.u32(3u).u32(1u).u32(64u).u32(64u);
  CHECK_EQ(fixture.send(other_2d), 0u);
  /* SET_SCANOUT_BLOB moved to ScanoutCommands (feature 058); BlobCommands
   * leaves it, and everything else it does not own, to the next answerer. */
  CHECK_EQ(fixture.send_type(0x010Du, GpuRequestBuilder(0x010Du)), 0u);
  CHECK_EQ(fixture.send_type(0x0100u, GpuRequestBuilder(0x0100u)), 0u); }

/* The chunk a guest process bound and imported: its pages are in a host API's
 * hands, and freeing them under it would hand the next blob's id the old pages. */
TEST(blob_commands_unref, a_blob_an_import_holds_is_refused_and_kept)
{ Fixture fixture;
  GpuRequestBuilder request = create_blob(4u, guest, 0u, 1u, 0u, 0x1000u);
  request.u64(ram).u32(0x1000u).u32(0u);
  CHECK_EQ(fixture.send(request), ok_nodata);
  const av::GuestPageAlias *alias = nullptr;
  av::BlobWindow            pages;
  CHECK_STATUS(fixture.blobs().hold_alias(4u, alias, pages), Status::ok);
  GpuRequestBuilder unref(BlobCommands::resource_unref);
  unref.u32(4u).u32(0u);
  CHECK_EQ(fixture.send(unref), err_invalid_parameter);
  CHECK(fixture.blobs().contains(4u));
  fixture.blobs().release_alias(4u);
  CHECK_EQ(fixture.send(unref), ok_nodata);
  CHECK(!fixture.blobs().contains(4u)); }

TEST(blob_commands_create, records_a_blob_created_and_a_blob_released)
{ Fixture fixture;
  CHECK_STATUS(fixture.recorder().open("blobs.rvk", 0u, "blob", av::AtCeiling::stop, av::GpuIdentity(),
                                       av::ProtocolVersion(1u, 1u)),
               Status::ok);
  GpuRequestBuilder request = create_blob(1u, guest, 0u, 2u, 0u, 0x3000u);
  request.u64(ram).u32(0x1000u).u32(0u).u64(ram + 0x4000u).u32(0x2000u).u32(0u);
  CHECK_EQ(fixture.send(request), ok_nodata);
  CHECK_EQ(fixture.send(create_blob(2u, host3d, mappable, 0u, 0u, 0x2000u)), ok_nodata);
  GpuRequestBuilder unref(BlobCommands::resource_unref);
  unref.u32(1u).u32(0u);
  CHECK_EQ(fixture.send(unref), ok_nodata);

  av::RecordedFile    file(fixture.host(), "blobs.rvk");
  av::RecordHeader    record;
  const std::uint8_t *payload = file.expect(av::RecordKind::blob, record);
  CHECK(payload != nullptr);
  if (payload != nullptr) {
      CHECK_EQ(payload[0], 0u);       /* created */
      CHECK_EQ(payload[8], 1u);       /* the guest blob's resource id */
      CHECK_EQ(payload[24], 0u);      /* its size, 0x3000 */
      CHECK_EQ(payload[25], 0x30u);
  }
  /* The blob record names no context: a blob belongs to the device. */
  CHECK_EQ(record.context(), 0u);
  payload = file.expect(av::RecordKind::blob, record);
  CHECK(payload != nullptr);
  if (payload != nullptr) {
      CHECK_EQ(payload[0], 0u);
      CHECK_EQ(payload[8], 2u);
  }
  payload = file.expect(av::RecordKind::blob, record);
  CHECK(payload != nullptr);
  if (payload != nullptr) {
      CHECK_EQ(payload[0], 2u);       /* released */
      CHECK_EQ(payload[8], 1u);
  }
  CHECK_EQ(file.remaining(), 0u);
  fixture.recorder().close(); }

TEST(blob_commands_create, a_creation_the_device_refused_records_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.recorder().open("refused.rvk", 0u, "blob", av::AtCeiling::stop, av::GpuIdentity(),
                                       av::ProtocolVersion(1u, 1u)),
               Status::ok);
  /* A host blob that is not mappable, and an unref of a resource nobody made. */
  CHECK_EQ(fixture.send(create_blob(3u, host3d, 0u, 0u, 0u, 0x2000u)), err_invalid_parameter);
  GpuRequestBuilder unref(BlobCommands::resource_unref);
  unref.u32(99u).u32(0u);
  static_cast<void>(fixture.send(unref));
  CHECK_EQ(av::RecordedFile(fixture.host(), "refused.rvk").remaining(), 0u);
  fixture.recorder().close(); }
