/* The requests a driver sends to use rvk contexts, read back by the device's
 * own decoders: the two sides agree on every byte. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/virtio_gpu/blob_request.hpp"
#include "proto/virtio_gpu/capset_request.hpp"
#include "proto/virtio_gpu/context_request.hpp"
#include "proto/virtio_gpu/create_blob_request.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/virtio_gpu/rvk_request_writer.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::BlobRequest;
using av::ByteLayout;
using av::CapsetRequest;
using av::GuestPageList;
using av::ContextRequest;
using av::ContextSetup;
using av::CreateBlobRequest;
using av::ProtocolVersion;
using av::RvkRequestWriter;
using av::Status;
using av::SubApi;
using av::WireReader;

namespace {

bool zero(const std::uint8_t *bytes, std::size_t from, std::size_t to)
{
    for (std::size_t i = from; i < to; i++)
        if (bytes[i] != 0u) return false;
    return true;
}

struct Buffer {
    Buffer() { std::memset(bytes, 0xEE, sizeof bytes); }
    std::uint8_t bytes[RvkRequestWriter::max_request_size + 8u];
};

}  // namespace

TEST(rvk_request_writer_capset_info, is_get_capset_info_for_the_index)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.capset_info(3u), Status::ok);
  CHECK_EQ(writer.length(), 32u);
  const ByteLayout layout(buffer.bytes, 32u);
  CHECK(layout.at(0u, 4u) == 0x0108u);
  CHECK(zero(buffer.bytes, 4u, 24u));
  WireReader reader(buffer.bytes, writer.length());
  CapsetRequest decoded;
  CHECK_STATUS(decoded.decode_info(reader), Status::ok);
  CHECK_EQ(decoded.index(), 3u);
  CHECK_EQ(buffer.bytes[32], 0xEEu); }

TEST(rvk_request_writer_capset, is_get_capset_for_the_id_and_version)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.capset(0x72u, 1u), Status::ok);
  CHECK_EQ(writer.length(), 32u);
  CHECK(ByteLayout(buffer.bytes, 32u).at(0u, 4u) == 0x0109u);
  WireReader reader(buffer.bytes, writer.length());
  CapsetRequest decoded;
  CHECK_STATUS(decoded.decode_capset(reader), Status::ok);
  CHECK_EQ(decoded.capset_id(), 0x72u);
  CHECK_EQ(decoded.capset_version(), 1u); }

TEST(rvk_request_writer_create_context, is_ctx_create_naming_the_capset_with_no_name)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.create_context(9u, 0x72u), Status::ok);
  CHECK_EQ(writer.length(), 96u);
  CHECK(ByteLayout(buffer.bytes, 96u).at(0u, 4u) == 0x0200u);
  CHECK(zero(buffer.bytes, 32u, 96u));
  WireReader reader(buffer.bytes, writer.length());
  ContextRequest decoded;
  CHECK_STATUS(decoded.decode_create(reader), Status::ok);
  CHECK_EQ(decoded.context_id(), 9u);
  CHECK_EQ(decoded.context_init(), 0x72u);
  CHECK_EQ(decoded.name_length(), 0u); }

TEST(rvk_request_writer_destroy_context, is_ctx_destroy)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.destroy_context(9u), Status::ok);
  CHECK_EQ(writer.length(), 24u);
  CHECK(ByteLayout(buffer.bytes, 24u).at(0u, 4u) == 0x0201u);
  WireReader reader(buffer.bytes, writer.length());
  ContextRequest decoded;
  CHECK_STATUS(decoded.decode_destroy(reader), Status::ok);
  CHECK_EQ(decoded.context_id(), 9u); }

TEST(rvk_request_writer_guest_blob, is_a_guest_blob_of_one_entry)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.guest_blob(40u, 0x0000000123456000ull, 0x2000u), Status::ok);
  CHECK_EQ(writer.length(), 72u);
  WireReader reader(buffer.bytes, writer.length());
  CreateBlobRequest decoded;
  CHECK_STATUS(decoded.decode(reader), Status::ok);
  CHECK_EQ(decoded.resource_id(), 40u);
  CHECK_EQ(decoded.memory(), CreateBlobRequest::memory_guest);
  CHECK_EQ(decoded.flags(), 0u);
  CHECK_EQ(decoded.entries(), 1u);
  CHECK(decoded.blob_id() == 0u);
  CHECK(decoded.size() == 0x2000u);
  const ByteLayout layout(buffer.bytes, 72u);
  CHECK(layout.at(56u, 8u) == 0x0000000123456000ull);
  CHECK(layout.at(64u, 4u) == 0x2000u);
  CHECK(layout.at(68u, 4u) == 0u); }

/* An aperture allocation's scattered pages become one blob with a memory entry
 * per run of adjacent frames, its size their total - two runs here, a lone page
 * then a pair. */
TEST(rvk_request_writer_guest_blob_pages, one_entry_per_run_of_adjacent_pages)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  const std::uint64_t frames[] = {0x11111000ull, 0x22222000ull, 0x22223000ull};
  const GuestPageList pages(frames, 3u);
  CHECK_STATUS(writer.guest_blob_pages(40u, pages), Status::ok);
  CHECK_EQ(writer.length(), 56u + 2u * 16u);
  WireReader reader(buffer.bytes, writer.length());
  CreateBlobRequest decoded;
  CHECK_STATUS(decoded.decode(reader), Status::ok);
  CHECK_EQ(decoded.resource_id(), 40u);
  CHECK_EQ(decoded.memory(), CreateBlobRequest::memory_guest);
  CHECK_EQ(decoded.entries(), 2u);
  CHECK(decoded.size() == 0x3000u);
  const ByteLayout layout(buffer.bytes, writer.length());
  CHECK(layout.at(56u, 8u) == 0x11111000ull);
  CHECK(layout.at(64u, 4u) == 0x1000u);
  CHECK(layout.at(72u, 8u) == 0x22222000ull);
  CHECK(layout.at(80u, 4u) == 0x2000u); }

/* A list with a page that is not page-aligned is not complete and is refused;
 * so is the empty list. */
TEST(rvk_request_writer_guest_blob_pages, refuses_an_incomplete_list)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  const std::uint64_t unaligned[] = {0x11111800ull};
  CHECK_STATUS(writer.guest_blob_pages(40u, GuestPageList(unaligned, 1u)),
               Status::invalid_parameter);
  CHECK_STATUS(writer.guest_blob_pages(40u, GuestPageList()), Status::invalid_parameter);
  CHECK_EQ(writer.length(), 0u); }

TEST(rvk_request_writer_unref, is_resource_unref)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.unref(41u), Status::ok);
  CHECK_EQ(writer.length(), 32u);
  CHECK(ByteLayout(buffer.bytes, 32u).at(0u, 4u) == 0x0102u);
  WireReader reader(buffer.bytes, writer.length());
  BlobRequest decoded;
  CHECK_STATUS(decoded.decode_resource(reader), Status::ok);
  CHECK_EQ(decoded.resource_id(), 41u); }

TEST(rvk_request_writer_setup, is_submit_3d_carrying_one_context_setup)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  const ContextSetup setup(ProtocolVersion(1u, 0u), SubApi::vulkan, 40u, 12u, 41u, 7u, 42u);
  CHECK_STATUS(writer.setup(9u, setup), Status::ok);
  CHECK_EQ(writer.length(), 64u);
  WireReader reader(buffer.bytes, writer.length());
  ContextRequest decoded;
  CHECK_STATUS(decoded.decode_submit(reader), Status::ok);
  CHECK_EQ(decoded.context_id(), 9u);
  CHECK_EQ(decoded.command_size(), ContextSetup::base_size);
  ContextSetup read_back;
  CHECK_STATUS(read_back.decode(buffer.bytes + 32u, ContextSetup::base_size), Status::ok);
  CHECK_EQ(read_back.doorbell(), 7u);
  CHECK_EQ(read_back.reply_resource(), 42u); }

/* A setup of the current minor carries its options in 48 bytes; one whose
 * options it cannot carry is refused and nothing is left to send. */
TEST(rvk_request_writer_setup, carries_a_setup_of_its_own_size)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  const av::GlContextOptions options(4u, 6u, av::GlContextOptions::Profile::core, 0u, 3u);
  const ContextSetup setup(ProtocolVersion(1u, 1u), SubApi::opengl, 40u, 12u, 41u, 7u, 42u, options);
  CHECK_STATUS(writer.setup(9u, setup), Status::ok);
  CHECK_EQ(writer.length(), 80u);
  WireReader reader(buffer.bytes, writer.length());
  ContextRequest decoded;
  CHECK_STATUS(decoded.decode_submit(reader), Status::ok);
  CHECK_EQ(decoded.command_size(), 48u);
  ContextSetup read_back;
  CHECK_STATUS(read_back.decode(buffer.bytes + 32u, 48u), Status::ok);
  CHECK_EQ(read_back.options().major(), std::uint8_t{4u});
  CHECK_EQ(read_back.options().share_context(), 3u);
  const ContextSetup vulkan_with_options(ProtocolVersion(1u, 1u), SubApi::vulkan, 40u, 12u, 41u, 7u, 42u, options);
  CHECK_STATUS(writer.setup(9u, vulkan_with_options), Status::invalid_parameter);
  CHECK_EQ(writer.length(), 0u);
  std::uint8_t tight[79] = {};
  RvkRequestWriter short_writer(tight, sizeof tight);
  CHECK_STATUS(short_writer.setup(9u, setup), Status::too_large);
  CHECK_EQ(short_writer.length(), 0u); }

TEST(rvk_request_writer, a_request_that_does_not_fit_writes_nothing)
{ std::uint8_t bytes[71];
  std::memset(bytes, 0xEE, sizeof bytes);
  RvkRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.capset_info(0u), Status::ok);
  CHECK_STATUS(writer.guest_blob(1u, 0u, 1u), Status::too_large);
  CHECK_EQ(writer.length(), 0u);
  CHECK_EQ(bytes[32], 0xEEu);
  CHECK_STATUS(writer.create_context(1u, 0x72u), Status::too_large);
  CHECK_STATUS(writer.setup(1u, ContextSetup()), Status::ok);
  RvkRequestWriter none(nullptr, 64u);
  CHECK_STATUS(none.destroy_context(1u), Status::too_large);
  CHECK_STATUS(none.capset(0x72u, 1u), Status::too_large);
  CHECK_STATUS(none.unref(1u), Status::too_large);
  CHECK_STATUS(none.setup(1u, ContextSetup()), Status::too_large); }

/* Feature 041: the three requests the driver had never sent. A host-visible
 * blob is the device's own shape - HOST3D, mappable, no memory entries - and
 * the offset it is mapped at is the guest's to choose, which is why the driver
 * has to be able to say it. */
TEST(rvk_request_writer_host_blob, is_a_mappable_host3d_blob_of_no_entries)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.host_blob(50u, 0x200000u), Status::ok);
  CHECK_EQ(writer.length(), 56u);
  CHECK(ByteLayout(buffer.bytes, 56u).at(0u, 4u) == 0x010Cu);
  WireReader reader(buffer.bytes, writer.length());
  CreateBlobRequest decoded;
  CHECK_STATUS(decoded.decode(reader), Status::ok);
  CHECK_EQ(decoded.resource_id(), 50u);
  CHECK_EQ(decoded.memory(), CreateBlobRequest::memory_host3d);
  CHECK_EQ(decoded.flags(), CreateBlobRequest::flag_mappable);
  CHECK_EQ(decoded.entries(), 0u);
  CHECK(decoded.blob_id() == 0u);
  CHECK(decoded.size() == 0x200000u); }

TEST(rvk_request_writer_map_blob, carries_the_offset_the_driver_chose)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.map_blob(50u, 0x4000u), Status::ok);
  CHECK_EQ(writer.length(), 40u);
  CHECK(ByteLayout(buffer.bytes, 40u).at(0u, 4u) == 0x0208u);
  WireReader reader(buffer.bytes, writer.length());
  BlobRequest decoded;
  CHECK_STATUS(decoded.decode_map(reader), Status::ok);
  CHECK_EQ(decoded.resource_id(), 50u);
  CHECK(decoded.offset() == 0x4000u); }

TEST(rvk_request_writer_unmap_blob, names_the_resource_alone)
{ Buffer buffer;
  RvkRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.unmap_blob(50u), Status::ok);
  CHECK_EQ(writer.length(), 32u);
  CHECK(ByteLayout(buffer.bytes, 32u).at(0u, 4u) == 0x0209u);
  WireReader reader(buffer.bytes, writer.length());
  BlobRequest decoded;
  CHECK_STATUS(decoded.decode_resource(reader), Status::ok);
  CHECK_EQ(decoded.resource_id(), 50u); }

/* A request that does not fit writes nothing, as every other one does. */
TEST(rvk_request_writer_host_blob, what_does_not_fit_is_not_written)
{ std::uint8_t bytes[40];
  std::memset(bytes, 0xEE, sizeof bytes);
  RvkRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.host_blob(1u, 0x1000u), Status::too_large);
  CHECK_EQ(writer.length(), 0u);
  CHECK_EQ(bytes[32], 0xEEu);
  CHECK_STATUS(writer.map_blob(1u, 0u), Status::ok);
  RvkRequestWriter small(bytes, 36u);
  CHECK_STATUS(small.map_blob(1u, 0u), Status::too_large);
  CHECK_EQ(small.length(), 0u);
  RvkRequestWriter none(nullptr, 64u);
  CHECK_STATUS(none.host_blob(1u, 0x1000u), Status::too_large);
  CHECK_STATUS(none.map_blob(1u, 0u), Status::too_large);
  CHECK_STATUS(none.unmap_blob(1u), Status::too_large); }
