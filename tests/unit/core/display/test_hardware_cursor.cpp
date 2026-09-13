#include "core/ddi/dxgkarg_set_pointer_position_record.hpp"
#include "core/ddi/dxgkarg_set_pointer_shape_record.hpp"
#include "core/display/display_sources.hpp"
#include "core/display/hardware_cursor.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::DxgkargSetPointerPositionRecord;
using av::DxgkargSetPointerShapeRecord;
using av::FakeVirtqueueDevice;
using av::HardwareCursor;
using av::Irql;
using av::ObjectStorage;
using av::Status;
using av::VirtioGpuDevice;
using av::Violation;

namespace {

constexpr std::uint32_t ok_nodata = 0x1100u;
constexpr std::uint32_t create_2d = 0x0101u, transfer = 0x0105u, attach = 0x0106u;
constexpr std::uint32_t update = 0x0300u, move = 0x0301u;
constexpr std::uint32_t color = 2u, visible = 1u;
constexpr std::uint32_t no_memory = 0xC0000017u;

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint32_t{bytes[offset]} | std::uint32_t{bytes[offset + 1u]} << 8u |
           std::uint32_t{bytes[offset + 2u]} << 16u | std::uint32_t{bytes[offset + 3u]} << 24u;
}

const std::uint8_t arrow[4 * 4u] = {1, 2, 3, 0xFF, 4, 5, 6, 0xFF, 7, 8, 9, 0xFF, 1, 1, 1, 0x80};

/* A started virtio-gpu device the cursor sends on, and a cursor with its backing. */
class Fixture {
public:
    Fixture() : mapped_(), storage_(), sources_(mapped_.platform()), cursor_(mapped_.platform(), sources_, storage_)
    {
        CHECK_STATUS(mapped_.map(), Status::ok);
        CHECK_STATUS(storage_.construct(mapped_.platform(), mapped_.mappings(), mapped_.layout()), Status::ok);
        CHECK_STATUS(storage_.object().start(3u), Status::ok);
        CHECK_STATUS(cursor_.initialize(), Status::ok);
    }
    ~Fixture()
    {
        cursor_.release();
        storage_.destroy();
    }

    av::MockPlatform                &platform() { return mapped_.platform(); }
    av::MappedVirtioFunction        &mapped() { return mapped_; }
    ObjectStorage<VirtioGpuDevice>  &storage() { return storage_; }
    HardwareCursor                  &cursor() { return cursor_; }

private:
    av::MappedVirtioFunction       mapped_;
    ObjectStorage<VirtioGpuDevice> storage_;
    av::DisplaySources             sources_;
    HardwareCursor                 cursor_;
};

DxgkargSetPointerShapeRecord shape(std::uint32_t hot_x = 1u, std::uint32_t hot_y = 0u)
{
    return DxgkargSetPointerShapeRecord{color, 2u, 2u, 8u, 0u, 0u, arrow, hot_x, hot_y};
}

}  // namespace

TEST(hardware_cursor_set_shape, creates_the_resource_once_then_transfers_each_shape)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  FakeVirtqueueDevice cursor = fixture.mapped().queue_device(1u);
  CHECK_STATUS(fixture.cursor().set_shape(shape()), Status::ok);
  CHECK_STATUS(fixture.cursor().set_shape(shape()), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 4u);
  CHECK_EQ(control.request_type(0u), create_2d);
  CHECK_EQ(u32(control.request(0u), 24u), HardwareCursor::resource_id);
  CHECK_EQ(u32(control.request(0u), 28u), 1u);    /* B8G8R8A8 */
  CHECK_EQ(u32(control.request(0u), 32u), 64u);
  CHECK_EQ(control.request_type(1u), attach);
  CHECK_EQ(u32(control.request(1u), 40u), 64u * 64u * 4u);
  const std::uint64_t backing = u32(control.request(1u), 32u) | std::uint64_t{u32(control.request(1u), 36u)} << 32u;
  const auto *pixels = static_cast<std::uint8_t *>(fixture.platform().translate_physical(backing, 64u * 64u * 4u));
  CHECK(pixels != nullptr);
  CHECK_EQ(pixels != nullptr ? pixels[4u * 65u + 3u] : 0u, 0x80u);
  CHECK_EQ(control.request_type(2u), transfer);
  CHECK_EQ(control.request_type(3u), transfer);
  CHECK_EQ(cursor.respond_all(ok_nodata), 0u); }

TEST(hardware_cursor_set_position, shows_moves_hides_and_shows_again)
{ Fixture fixture;
  FakeVirtqueueDevice cursor = fixture.mapped().queue_device(1u);
  CHECK_STATUS(fixture.cursor().set_shape(shape(1u, 1u)), Status::ok);
  const DxgkargSetPointerPositionRecord here = {0u, 100, 200, visible};
  const DxgkargSetPointerPositionRecord there = {0u, -5, 7, visible};
  const DxgkargSetPointerPositionRecord hidden = {0u, 0, 0, 0u};
  CHECK_STATUS(fixture.cursor().set_position(here), Status::ok);
  CHECK_STATUS(fixture.cursor().set_position(there), Status::ok);
  CHECK_STATUS(fixture.cursor().set_position(hidden), Status::ok);
  CHECK_STATUS(fixture.cursor().set_position(here), Status::ok);
  CHECK_STATUS(fixture.cursor().set_shape(shape()), Status::ok);
  CHECK_EQ(cursor.respond_all(ok_nodata), 5u);
  CHECK_EQ(cursor.request_type(0u), update);
  CHECK_EQ(u32(cursor.request(0u), 28u), 100u);
  CHECK_EQ(u32(cursor.request(0u), 40u), HardwareCursor::resource_id);
  CHECK_EQ(u32(cursor.request(0u), 44u), 1u);
  CHECK_EQ(u32(cursor.request(0u), 48u), 1u);
  CHECK_EQ(cursor.request_type(1u), move);
  CHECK_EQ(u32(cursor.request(1u), 28u), 0xFFFFFFFBu);
  CHECK_EQ(cursor.request_type(2u), update);
  CHECK_EQ(u32(cursor.request(2u), 40u), 0u);
  CHECK_EQ(cursor.request_type(3u), update);
  CHECK_EQ(u32(cursor.request(3u), 40u), HardwareCursor::resource_id);
  CHECK_EQ(cursor.request_type(4u), update);
  CHECK_EQ(u32(cursor.request(4u), 44u), 1u);
  CHECK_EQ(u32(cursor.request(4u), 48u), 0u); }

TEST(hardware_cursor_set_position, without_a_shape_nothing_is_shown)
{ Fixture fixture;
  FakeVirtqueueDevice cursor = fixture.mapped().queue_device(1u);
  const DxgkargSetPointerPositionRecord here = {0u, 1, 2, visible};
  CHECK_STATUS(fixture.cursor().set_position(here), Status::ok);
  CHECK_EQ(cursor.respond_all(ok_nodata), 0u);
  CHECK_STATUS(fixture.cursor().set_shape(shape()), Status::ok);
  CHECK_EQ(cursor.respond_all(ok_nodata), 1u);
  CHECK_EQ(cursor.request_type(0u), update);
  CHECK_EQ(u32(cursor.request(0u), 32u), 2u); }

TEST(hardware_cursor_forget_device, the_next_shape_creates_the_resource_again)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.cursor().set_shape(shape()), Status::ok);
  const DxgkargSetPointerPositionRecord here = {0u, 1, 2, visible};
  CHECK_STATUS(fixture.cursor().set_position(here), Status::ok);
  fixture.cursor().forget_device();
  CHECK_STATUS(fixture.cursor().set_shape(shape()), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 6u);
  CHECK_EQ(control.request_type(3u), create_2d);
  FakeVirtqueueDevice cursor = fixture.mapped().queue_device(1u);
  CHECK_EQ(cursor.respond_all(ok_nodata), 1u); }

TEST(hardware_cursor, refuses_without_a_device_backing_or_valid_arguments)
{ Fixture fixture;
  const DxgkargSetPointerPositionRecord other_source = {1u, 0, 0, visible};
  DxgkargSetPointerShapeRecord bad = shape();
  bad.Width = 0u;
  CHECK_STATUS(fixture.cursor().set_shape(bad), Status::invalid_parameter);
  bad = shape();
  bad.VidPnSourceId = 1u;
  CHECK_STATUS(fixture.cursor().set_shape(bad), Status::invalid_parameter);
  CHECK_STATUS(fixture.cursor().set_position(other_source), Status::invalid_parameter);
  fixture.cursor().release();
  CHECK_STATUS(fixture.cursor().set_shape(shape()), Status::invalid_state);
  CHECK_STATUS(fixture.cursor().initialize(), Status::ok);
  CHECK_STATUS(fixture.cursor().initialize(), Status::invalid_state);
  fixture.storage().destroy();
  CHECK_STATUS(fixture.cursor().set_shape(shape()), Status::invalid_state);
  CHECK_STATUS(fixture.cursor().set_position(other_source), Status::invalid_state); }

TEST(hardware_cursor_set_shape, a_full_queue_or_stopped_device_fails_each_step)
{ Fixture fixture;
  CHECK_STATUS(fixture.storage().object().stop(), Status::ok);
  CHECK_STATUS(fixture.cursor().set_shape(shape()), Status::invalid_state);
  const DxgkargSetPointerPositionRecord here = {0u, 1, 2, visible};
  const DxgkargSetPointerPositionRecord hidden = {0u, 0, 0, 0u};
  CHECK_STATUS(fixture.cursor().set_position(hidden), Status::invalid_state);
  CHECK_STATUS(fixture.cursor().set_position(here), Status::ok); }

TEST(hardware_cursor_set_shape, a_refused_attach_or_transfer_is_reported)
{ const std::uint32_t free_slots[] = {1u, 2u};
  for (std::uint32_t free : free_slots) {
      Fixture fixture;
      const std::uint8_t filler[8] = {1};
      for (std::uint32_t slot = 0u; slot < av::GpuCommandQueue::slot_count - free; slot++)
          CHECK_STATUS(fixture.storage().object().send_control(filler, sizeof filler), Status::ok);
      CHECK_STATUS(fixture.cursor().set_shape(shape()), Status::no_space);
  } }

TEST(hardware_cursor_initialize, out_of_memory)
{ av::MappedVirtioFunction mapped;
  ObjectStorage<VirtioGpuDevice> storage;
  const av::DisplaySources sources(mapped.platform());
  HardwareCursor cursor(mapped.platform(), sources, storage);
  mapped.platform().fail_allocation_at(1u);
  CHECK_STATUS(cursor.initialize(), Status::no_memory);
  CHECK_EQ(no_memory, 0xC0000017u); }

TEST(hardware_cursor, ceilings)
{ Fixture fixture;
  const DxgkargSetPointerPositionRecord hidden = {5u, 0, 0, 0u};
  DxgkargSetPointerShapeRecord bad = shape();
  bad.VidPnSourceId = 5u;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.cursor().set_shape(bad));
  static_cast<void>(fixture.cursor().set_position(hidden));
  fixture.cursor().forget_device();
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.cursor().initialize());
  fixture.cursor().release();
  fixture.platform().set_irql(Irql::passive);
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::allocation_at_dispatch)) {}
  CHECK_STATUS(fixture.cursor().initialize(), Status::ok); }
