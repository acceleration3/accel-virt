/* The device's host and the renderer's service in one process, joined by
 * MockHost's renderer socket: what QEMU sends crosses as bytes and duplicated
 * descriptors and comes back as the renderer's answer, so the protocol is
 * proven end to end before a process boundary is involved. */
#include "core/support/status.hpp"
#include "device/renderer/guest_memory_table.hpp"
#include "device/renderer/renderer_service.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/loopback_renderer_peer.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/device_uuid.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/renderer/hello_message.hpp"
#include "proto/renderer/ready_message.hpp"
#include "proto/renderer/region_message.hpp"
#include "proto/renderer/renderer_message_header.hpp"

#include <cstdint>
#include <cstring>

using av::LoopbackRendererPeer;
using av::MockExecutorOps;
using av::MockHost;
using av::RendererMessageHeader;
using av::Status;

namespace {

constexpr char        executor_path[] = "/opt/av/libav_executor.so";
constexpr std::size_t datagram_capacity = RendererMessageHeader::encoded_size + 4096u + 256u;

/* The hello QEMU sends a new renderer: protocol 1, the bound GPU's UUID, the
 * library to load and a capset offering Vulkan. */
std::size_t hello(std::uint8_t (&datagram)[datagram_capacity], std::uint64_t sequence)
{
    const std::uint8_t uuid[av::HelloMessage::uuid_bytes] = {0x42u, 7u};
    const av::CapsetInfo capset(av::ProtocolVersion(av::ProtocolVersion::current_major,
                                                    av::ProtocolVersion::current_minor),
                                static_cast<std::uint32_t>(av::SubApi::vulkan), 64u, 12u, 24u, 0u,
                                av::GpuIdentity(0x1002u, 0x7550u, av::GpuIdentity::discrete_gpu, uuid, 1u, 1u,
                                                "mock gpu"));
    std::uint8_t capset_bytes[av::CapsetInfo::encoded_size] = {};
    CHECK_STATUS(capset.encode(capset_bytes, sizeof capset_bytes), Status::ok);
    const av::HelloMessage message(av::HelloMessage::protocol_version, 1u, av::DeviceUuid(uuid), executor_path,
                                   sizeof executor_path - 1u, capset_bytes, sizeof capset_bytes);
    const RendererMessageHeader header(RendererMessageHeader::Kind::hello,
                                       static_cast<std::uint32_t>(message.encoded_length()), sequence);
    CHECK_STATUS(header.encode(datagram, sizeof datagram), Status::ok);
    CHECK_STATUS(message.encode(datagram + RendererMessageHeader::encoded_size,
                                sizeof datagram - RendererMessageHeader::encoded_size),
                 Status::ok);
    return RendererMessageHeader::encoded_size + message.encoded_length();
}

std::size_t region(std::uint8_t (&datagram)[datagram_capacity], std::uint32_t number, std::uint64_t size,
                   std::uint64_t sequence)
{
    const av::RegionMessage     message(number, size);
    const RendererMessageHeader header(RendererMessageHeader::Kind::region, av::RegionMessage::encoded_size,
                                       sequence);
    CHECK_STATUS(header.encode(datagram, sizeof datagram), Status::ok);
    CHECK_STATUS(message.encode(datagram + RendererMessageHeader::encoded_size,
                                sizeof datagram - RendererMessageHeader::encoded_size),
                 Status::ok);
    return RendererMessageHeader::encoded_size + av::RegionMessage::encoded_size;
}

}  // namespace

TEST(renderer_loopback, a_hello_loads_the_executor_and_is_answered_ready)
{ MockHost qemu;
  MockExecutorOps executor(static_cast<std::uint32_t>(av::SubApi::vulkan));
  LoopbackRendererPeer peer(qemu, executor.table());
  qemu.connect_renderer(peer);
  CHECK_STATUS(qemu.start_renderer("av-renderer"), Status::ok);
  std::uint8_t datagram[datagram_capacity] = {};
  const std::size_t length = hello(datagram, 9u);
  CHECK_STATUS(qemu.send_renderer(datagram, length, nullptr, 0u), Status::ok);
  CHECK_STATUS(peer.last_outcome(), Status::ok);
  CHECK(peer.service().ready());
  CHECK_EQ(peer.resources().executor_loads(), 1u);
  CHECK(std::strcmp(peer.resources().executor_path(), executor_path) == 0);

  std::uint8_t  answer[datagram_capacity] = {};
  std::size_t   answered = 0u;
  std::uint32_t count = 7u;
  CHECK_STATUS(qemu.receive_renderer(answer, sizeof answer, answered, nullptr, 0u, count, 100u), Status::ok);
  CHECK_EQ(count, 0u);
  RendererMessageHeader header;
  CHECK_STATUS(header.decode(answer, answered), Status::ok);
  CHECK(header.kind() == RendererMessageHeader::Kind::ready);
  CHECK_EQ(header.sequence(), 9u);
  av::ReadyMessage ready;
  CHECK_STATUS(ready.decode(answer + RendererMessageHeader::encoded_size, header.body_length(), 0u), Status::ok);
  CHECK_EQ(ready.version(), av::HelloMessage::protocol_version);
  CHECK_EQ(ready.executor_abi(), static_cast<std::uint32_t>(AV_EXECUTOR_ABI_VERSION));
  CHECK_EQ(ready.sub_apis(), static_cast<std::uint32_t>(av::SubApi::vulkan));
  /* One hello, one answer: the next receive has nothing to take. */
  CHECK_STATUS(qemu.receive_renderer(answer, sizeof answer, answered, nullptr, 0u, count, 100u),
               Status::invalid_state);

  /* A second hello is not a renderer's to accept, and is answered nothing. */
  CHECK_STATUS(qemu.send_renderer(datagram, length, nullptr, 0u), Status::ok);
  CHECK_STATUS(peer.last_outcome(), Status::malformed);
  CHECK_EQ(peer.queued(), 0u);
  CHECK_EQ(peer.resources().executor_loads(), 1u);
  CHECK(peer.service().invariants_hold()); }

/* A region's memfd crosses as the renderer's own copy; the renderer maps it
 * and a guest extent resolves to the very bytes QEMU registered, while the
 * device's descriptor is still its own to close. */
TEST(renderer_loopback, a_guest_region_maps_to_the_bytes_qemu_registered)
{ MockHost qemu;
  MockExecutorOps executor(static_cast<std::uint32_t>(av::SubApi::vulkan));
  LoopbackRendererPeer peer(qemu, executor.table());
  qemu.connect_renderer(peer);
  CHECK_STATUS(qemu.start_renderer("av-renderer"), Status::ok);
  std::uint8_t datagram[datagram_capacity] = {};
  CHECK_STATUS(qemu.send_renderer(datagram, hello(datagram, 1u), nullptr, 0u), Status::ok);
  std::uint8_t  answer[datagram_capacity] = {};
  std::size_t   answered = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(qemu.receive_renderer(answer, sizeof answer, answered, nullptr, 0u, count, 100u), Status::ok);

  std::uint8_t ram[0x2000] = {};
  ram[0x10] = 0xC3u;
  qemu.add_guest_region(2u, ram, sizeof ram);
  std::uint32_t number = 0u;
  int           descriptor = -1;
  std::uint64_t size = 0u;
  CHECK_STATUS(qemu.guest_region(0u, number, descriptor, size), Status::ok);
  CHECK_STATUS(qemu.send_renderer(datagram, region(datagram, number, size, 2u), &descriptor, 1u), Status::ok);
  CHECK_STATUS(peer.last_outcome(), Status::ok);
  CHECK(qemu.descriptor_open(descriptor));
  qemu.close_descriptor(descriptor);
  CHECK_EQ(qemu.live_descriptors(), 0u);
  CHECK_EQ(peer.memory().count(), 1u);
  const auto *const seen = static_cast<const std::uint8_t *>(
      peer.memory().resolve(static_cast<std::uint64_t>(2u) << av::GuestMemoryTable::region_shift | 0x10u, 1u));
  CHECK(seen == ram + 0x10);
  CHECK_EQ(*seen, 0xC3u);
  CHECK_EQ(peer.resources().live_mappings(), 1u);
  CHECK_EQ(peer.resources().live_descriptors(), 1u);   /* the table keeps the region's */
  CHECK_EQ(peer.queued(), 0u);                         /* a region is answered nothing */
  CHECK(peer.service().invariants_hold()); }

/* Before a hello nothing is taken, and whatever came with the datagram is
 * closed on the renderer's side rather than left open there. */
TEST(renderer_loopback, a_datagram_before_the_hello_is_refused_and_its_descriptor_closed)
{ MockHost qemu;
  MockExecutorOps executor(static_cast<std::uint32_t>(av::SubApi::vulkan));
  LoopbackRendererPeer peer(qemu, executor.table());
  qemu.connect_renderer(peer);
  CHECK_STATUS(qemu.start_renderer("av-renderer"), Status::ok);
  std::uint8_t ram[0x1000] = {};
  qemu.add_guest_region(0u, ram, sizeof ram);
  std::uint32_t number = 0u;
  int           descriptor = -1;
  std::uint64_t size = 0u;
  CHECK_STATUS(qemu.guest_region(0u, number, descriptor, size), Status::ok);
  std::uint8_t datagram[datagram_capacity] = {};
  CHECK_STATUS(qemu.send_renderer(datagram, region(datagram, number, size, 1u), &descriptor, 1u), Status::ok);
  CHECK_STATUS(peer.last_outcome(), Status::invalid_state);
  CHECK_EQ(peer.resources().live_descriptors(), 0u);
  CHECK_EQ(peer.memory().count(), 0u);
  qemu.close_descriptor(descriptor);
  CHECK_EQ(qemu.live_descriptors(), 0u); }
