#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/context/local_context_plane.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "device/renderer/doorbell_offers.hpp"
#include "device/renderer/guest_memory_table.hpp"
#include "device/renderer/renderer_host_blobs.hpp"
#include "device/renderer/renderer_service.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_renderer_resources.hpp"
#include "harness/context/guest_ring.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/device_uuid.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/renderer/bind_context_message.hpp"
#include "proto/renderer/allocate_memory_message.hpp"
#include "proto/renderer/allocated_memory_message.hpp"
#include "proto/renderer/blob_retired_message.hpp"
#include "proto/renderer/free_memory_message.hpp"
#include "proto/renderer/context_message.hpp"
#include "proto/renderer/export_image_message.hpp"
#include "proto/renderer/exported_image_message.hpp"
#include "proto/renderer/guest_blob_message.hpp"
#include "proto/renderer/hello_message.hpp"
#include "proto/renderer/interrupt_lines_message.hpp"
#include "proto/renderer/ready_message.hpp"
#include "proto/renderer/region_message.hpp"
#include "proto/renderer/renderer_extent.hpp"
#include "proto/renderer/renderer_message_header.hpp"
#include "proto/renderer/reply_message.hpp"

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

using av::MockExecutorOps;
using av::MockHost;
using av::RendererMessageHeader;
using av::RendererService;
using av::Status;
using Kind = av::RendererMessageHeader::Kind;

namespace av {

struct RendererServiceProbe {
    static BlobTable         &blobs(RendererService &service) { return service.blobs_; }
    static LocalContextPlane &plane(RendererService &service) { return service.plane_; }
    static RendererHostBlobs &owners(RendererService &service) { return service.owners_; }
    static void               forget_hello(RendererService &service) { service.ready_ = false; }
};

}

using av::RendererServiceProbe;

namespace {

constexpr std::uint32_t both = AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENGL;
constexpr char          library[] = "/opt/av/libav_executor.so";
constexpr std::uint8_t  uuid[av::HelloMessage::uuid_bytes] = {0x42u, 7u};
constexpr std::uint32_t region_number = 3u;
constexpr std::uint64_t region_size = 0x8000u;
constexpr std::uint32_t ring = 11u, fence = 12u, context_id = 4u, doorbell = 6u;
constexpr std::size_t   capacity = av::RendererMessageHeader::encoded_size + 4096u + 256u;

struct Published {
    bool             found;
    AvExportedImage image;
};
Published published = {};

std::uint32_t export_published(void *, std::uint32_t, std::uint64_t key, AvExportedImage *image)
{
    if (!published.found || key != 0x77u) return AV_UNKNOWN;
    *image = published.image;
    return AV_EXECUTED;
}

av::CapsetInfo capset()
{
    return av::CapsetInfo(av::ProtocolVersion(av::ProtocolVersion::current_major, av::ProtocolVersion::current_minor),
                          both, 64u, 12u, 24u, 0u,
                          av::GpuIdentity(0x1002u, 0x7550u, av::GpuIdentity::discrete_gpu, uuid, 1u, 1u, "mock gpu"));
}

int fresh_descriptor()
{
    int ends[2] = {-1, -1};
    if (::pipe2(ends, O_CLOEXEC) != 0) return -1;
    ::close(ends[1]);
    return ends[0];
}

bool is_open(int descriptor) { return ::fcntl(descriptor, F_GETFD) != -1; }

class Rig {
public:
    Rig()
        : ops_(both), resources_(qemu_, ops_.table()), service_(host_, resources_, memory_, doorbells_), ram_{}, answer_{},
          answer_length_(0u), answer_descriptor_(-1)
    {
        host_.translate_guest_with(
            [](void *context, std::uint64_t address, std::uint64_t length) -> void * {
                return static_cast<av::GuestMemoryTable *>(context)->resolve(address, length);
            },
            &memory_);
        host_.forward_requests();
        qemu_.add_guest_region(region_number, ram_, region_size);
    }
    ~Rig()
    {
        service_.shut_down();
        void         *base = nullptr;
        std::uint64_t size = 0u;
        int           descriptor = -1;
        if (memory_.remove(region_number, base, size, descriptor) == Status::ok) {
            resources_.unmap_region(base, size);
            resources_.close_descriptor(descriptor);
        }
    }

    MockHost                  &host() { return host_; }
    MockHost                  &qemu() { return qemu_; }
    MockExecutorOps           &ops() { return ops_; }
    av::MockRendererResources &resources() { return resources_; }
    av::GuestMemoryTable      &memory() { return memory_; }
    av::DoorbellOffers        &doorbells() { return doorbells_; }
    RendererService           &service() { return service_; }
    std::uint8_t              *ram() { return ram_; }
    const std::uint8_t        *answer() const { return answer_; }
    std::size_t                answer_length() const { return answer_length_; }
    int                        answer_descriptor() const { return answer_descriptor_; }

    Status send(Kind kind, std::uint64_t sequence, const std::uint8_t *body, std::uint32_t length,
                const int *descriptors = nullptr, std::uint32_t count = 0u)
    {
        std::uint8_t datagram[capacity] = {};
        static_cast<void>(RendererMessageHeader(kind, length, sequence).encode(datagram, sizeof datagram));
        if (length != 0u) std::memcpy(datagram + RendererMessageHeader::encoded_size, body, length);
        return raw(datagram, RendererMessageHeader::encoded_size + length, descriptors, count);
    }
    Status raw(const std::uint8_t *datagram, std::size_t length, const int *descriptors, std::uint32_t count,
               std::size_t room = RendererService::answer_capacity)
    {
        resources_.adopt(descriptors, count);
        return service_.receive(datagram, length, descriptors, count, answer_, room, answer_length_,
                                answer_descriptor_);
    }
    Status allocate(std::uint32_t context, std::uint64_t key, std::uint64_t size, std::uint64_t sequence = 5u,
                    std::size_t room = RendererService::answer_capacity)
    {
        std::uint8_t datagram[RendererMessageHeader::encoded_size + av::AllocateMemoryMessage::encoded_size] = {};
        static_cast<void>(RendererMessageHeader(Kind::allocate_memory, av::AllocateMemoryMessage::encoded_size, sequence)
                              .encode(datagram, sizeof datagram));
        static_cast<void>(av::AllocateMemoryMessage(context, 0x101u, key, size)
                              .encode(datagram + RendererMessageHeader::encoded_size,
                                      av::AllocateMemoryMessage::encoded_size));
        return raw(datagram, sizeof datagram, nullptr, 0u, room);
    }
    Status free_memory(std::uint64_t key)
    {
        std::uint8_t body[av::FreeMemoryMessage::encoded_size] = {};
        static_cast<void>(av::FreeMemoryMessage(key).encode(body, sizeof body));
        return send(Kind::free_memory, 6u, body, sizeof body);
    }
    av::AllocatedMemoryMessage allocated(std::uint64_t sequence = 5u)
    {
        RendererMessageHeader      header;
        av::AllocatedMemoryMessage message;
        CHECK_STATUS(header.decode(answer_, answer_length_), Status::ok);
        CHECK(header.kind() == Kind::allocated_memory);
        CHECK_EQ(header.sequence(), sequence);
        CHECK_STATUS(message.decode(answer_ + RendererMessageHeader::encoded_size, header.body_length(),
                                    answer_descriptor_ >= 0 ? 1u : 0u),
                     Status::ok);
        return message;
    }
    Status hello(std::uint64_t sequence = 1u)
    {
        std::uint8_t capset_bytes[av::CapsetInfo::encoded_size] = {};
        static_cast<void>(capset().encode(capset_bytes, sizeof capset_bytes));
        const av::HelloMessage message(av::HelloMessage::protocol_version, 1u, av::DeviceUuid(uuid), library, sizeof library - 1u,
                                       capset_bytes, sizeof capset_bytes);
        std::uint8_t body[capacity] = {};
        static_cast<void>(message.encode(body, sizeof body));
        return send(Kind::hello, sequence, body, static_cast<std::uint32_t>(message.encoded_length()));
    }
    Status region()
    {
        std::uint32_t number = 0u;
        int           descriptor = -1;
        std::uint64_t size = 0u;
        static_cast<void>(qemu_.guest_region(0u, number, descriptor, size));
        const int crossed = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 0);
        qemu_.close_descriptor(descriptor);
        std::uint8_t body[av::RegionMessage::encoded_size] = {};
        static_cast<void>(av::RegionMessage(number, size).encode(body, sizeof body));
        return send(Kind::region, 2u, body, sizeof body, &crossed, 1u);
    }
    Status guest_blob(std::uint32_t id, std::uint64_t offset, std::uint64_t size)
    {
        const av::RendererExtent   extent(region_number, offset, size);
        const av::GuestBlobMessage message(id, size, &extent, 1u);
        std::uint8_t               body[256] = {};
        static_cast<void>(message.encode(body, sizeof body));
        return send(Kind::guest_blob, 3u, body, static_cast<std::uint32_t>(message.encoded_length()));
    }
    Status context(Kind kind, std::uint32_t id, std::uint64_t sequence)
    {
        std::uint8_t body[av::ContextMessage::encoded_size] = {};
        static_cast<void>(av::ContextMessage(id).encode(body, sizeof body));
        return send(kind, sequence, body, sizeof body);
    }
    Status bind(std::uint32_t id, std::uint32_t page, int descriptor, std::uint64_t sequence)
    {
        const av::ContextSetup setup(av::ProtocolVersion(1u, 0u), av::SubApi::vulkan, ring, 12u, fence, page, 0u);
        std::uint8_t           setup_bytes[av::ContextSetup::encoded_size] = {};
        static_cast<void>(setup.encode(setup_bytes, sizeof setup_bytes));
        const av::BindContextMessage message(id, setup_bytes, static_cast<std::uint32_t>(setup.size()), page);
        std::uint8_t                 body[128] = {};
        static_cast<void>(message.encode(body, sizeof body));
        return send(Kind::bind_context, sequence, body, static_cast<std::uint32_t>(message.encoded_length()),
                    &descriptor, 1u);
    }
    void rings()
    {
        av::GuestRing(ram_, 12u);
        CHECK_STATUS(guest_blob(ring, 0u, 0x2000u), Status::ok);
        CHECK_STATUS(guest_blob(fence, 0x2000u, 0x1000u), Status::ok);
    }
    av::ReplyMessage reply(std::uint64_t &sequence)
    {
        RendererMessageHeader header;
        av::ReplyMessage      message;
        sequence = 0u;
        if (header.decode(answer_, answer_length_) == Status::ok && header.kind() == Kind::reply &&
            message.decode(answer_ + RendererMessageHeader::encoded_size, header.body_length(), 0u) == Status::ok)
            sequence = header.sequence();
        return message;
    }

private:
    MockHost                  qemu_;
    MockHost                  host_;
    MockExecutorOps           ops_;
    av::MockRendererResources resources_;
    av::GuestMemoryTable      memory_;
    av::DoorbellOffers        doorbells_;
    RendererService           service_;
    alignas(4096) std::uint8_t ram_[region_size];
    std::uint8_t              answer_[RendererService::answer_capacity];
    std::size_t               answer_length_;
    int                       answer_descriptor_;
};

}

TEST(renderer_service_hello, answers_ready_with_the_librarys_sub_apis)
{ Rig rig;
  CHECK_STATUS(rig.hello(9u), Status::ok);
  CHECK(rig.service().ready());
  CHECK_EQ(rig.resources().executor_loads(), 1u);
  CHECK(std::strcmp(rig.resources().executor_path(), library) == 0);
  RendererMessageHeader header;
  CHECK_STATUS(header.decode(rig.answer(), rig.answer_length()), Status::ok);
  CHECK(header.kind() == Kind::ready);
  CHECK_EQ(header.sequence(), 9u);
  av::ReadyMessage ready;
  CHECK_STATUS(ready.decode(rig.answer() + RendererMessageHeader::encoded_size, header.body_length(), 0u), Status::ok);
  CHECK_EQ(ready.version(), av::HelloMessage::protocol_version);
  CHECK_EQ(ready.executor_abi(), static_cast<std::uint32_t>(AV_EXECUTOR_ABI_VERSION));
  CHECK_EQ(ready.sub_apis(), both);
  CHECK_EQ(rig.answer_descriptor(), -1);
  CHECK(rig.service().invariants_hold());
  CHECK_STATUS(rig.hello(10u), Status::malformed);
  CHECK_EQ(rig.answer_length(), 0u);
  CHECK_EQ(rig.resources().executor_loads(), 1u); }

TEST(renderer_service_hello, refuses_another_version_a_bad_capset_or_descriptors)
{ Rig rig;
  std::uint8_t capset_bytes[av::CapsetInfo::encoded_size] = {};
  CHECK_STATUS(capset().encode(capset_bytes, sizeof capset_bytes), Status::ok);
  std::uint8_t body[capacity] = {};
  const av::HelloMessage other(av::HelloMessage::protocol_version + 1u, 1u, av::DeviceUuid(uuid), library, sizeof library - 1u,
                               capset_bytes, sizeof capset_bytes);
  CHECK_STATUS(other.encode(body, sizeof body), Status::ok);
  CHECK_STATUS(rig.send(Kind::hello, 1u, body, static_cast<std::uint32_t>(other.encoded_length())), Status::malformed);
  std::uint8_t garbage_capset[av::CapsetInfo::encoded_size] = {};
  std::memset(garbage_capset, 0xAB, sizeof garbage_capset);
  const av::HelloMessage bad_capset(av::HelloMessage::protocol_version, 1u, av::DeviceUuid(uuid), library, sizeof library - 1u,
                                    garbage_capset, sizeof garbage_capset);
  CHECK_STATUS(bad_capset.encode(body, sizeof body), Status::ok);
  CHECK_STATUS(rig.send(Kind::hello, 1u, body, static_cast<std::uint32_t>(bad_capset.encoded_length())),
               Status::malformed);
  const av::HelloMessage good(av::HelloMessage::protocol_version, 1u, av::DeviceUuid(uuid), library, sizeof library - 1u, capset_bytes,
                              sizeof capset_bytes);
  CHECK_STATUS(good.encode(body, sizeof body), Status::ok);
  const int carried[2] = {fresh_descriptor(), fresh_descriptor()};
  CHECK_STATUS(rig.send(Kind::hello, 1u, body, static_cast<std::uint32_t>(good.encoded_length()), carried, 2u),
               Status::malformed);
  CHECK(!is_open(carried[0]));
  CHECK(!is_open(carried[1]));
  CHECK_EQ(rig.resources().live_descriptors(), 0u);
  CHECK_EQ(rig.resources().executor_loads(), 0u);
  CHECK(!rig.service().ready());
  CHECK_EQ(rig.answer_length(), 0u); }

TEST(renderer_service_hello, a_library_that_does_not_load_or_is_refused_answers_nothing)
{ { Rig rig;
    rig.resources().refuse_executor();
    CHECK_STATUS(rig.hello(), Status::not_found);
    CHECK(!rig.service().ready());
    CHECK_EQ(rig.answer_length(), 0u);
    CHECK_STATUS(rig.hello(), Status::ok);
    CHECK(rig.service().ready()); }
  { Rig rig;
    rig.ops().editable().abi_version = 0u;
    CHECK_STATUS(rig.hello(), Status::not_supported);
    CHECK(!rig.service().ready()); }
  for (std::uint32_t lock = 1u; lock <= 3u; lock++) {
      Rig rig;
      rig.host().fail_mutex_at(lock);
      CHECK_STATUS(rig.hello(), Status::no_memory);
      CHECK(!rig.service().ready());
      CHECK_EQ(rig.answer_length(), 0u);
  }
  Rig rig;
  std::uint8_t capset_bytes[av::CapsetInfo::encoded_size] = {};
  CHECK_STATUS(capset().encode(capset_bytes, sizeof capset_bytes), Status::ok);
  const av::HelloMessage message(av::HelloMessage::protocol_version, 1u, av::DeviceUuid(uuid), library, sizeof library - 1u,
                                 capset_bytes, sizeof capset_bytes);
  std::uint8_t datagram[capacity] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::hello, static_cast<std::uint32_t>(message.encoded_length()), 1u)
                   .encode(datagram, sizeof datagram),
               Status::ok);
  CHECK_STATUS(message.encode(datagram + RendererMessageHeader::encoded_size, sizeof datagram - 16u), Status::ok);
  std::uint8_t small[RendererMessageHeader::encoded_size + av::ReadyMessage::encoded_size - 1u] = {};
  std::size_t  answered = 7u;
  int          descriptor = 7;
  CHECK_STATUS(rig.service().receive(datagram, RendererMessageHeader::encoded_size + message.encoded_length(), nullptr,
                                     0u, small, sizeof small, answered, descriptor),
               Status::too_large);
  CHECK_EQ(answered, 0u);
  CHECK_EQ(descriptor, -1);
  CHECK(!rig.service().ready()); }

TEST(renderer_service_receive, nothing_but_a_hello_or_interrupts_off_is_taken_first)
{ Rig rig;
  CHECK_STATUS(rig.region(), Status::invalid_state);
  CHECK_EQ(rig.resources().live_descriptors(), 0u);
  CHECK_EQ(rig.memory().count(), 0u);
  CHECK_STATUS(rig.context(Kind::create_context, context_id, 5u), Status::invalid_state);
  CHECK_EQ(rig.answer_length(), 0u);
  const int stray = fresh_descriptor();
  CHECK_STATUS(rig.send(Kind::interrupts_off, 6u, nullptr, 0u, &stray, 1u), Status::ok);
  CHECK(!is_open(stray));
  CHECK_EQ(rig.resources().live_descriptors(), 0u);
  CHECK(rig.service().invariants_hold()); }

TEST(renderer_service_receive, garbage_and_kinds_qemu_never_sends_are_malformed_and_closed)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  const int      first = fresh_descriptor();
  const std::uint8_t garbage[20] = {0xFFu};
  CHECK_STATUS(rig.raw(garbage, sizeof garbage, &first, 1u), Status::malformed);
  CHECK(!is_open(first));
  std::size_t answered = 0u;
  int         descriptor = -1;
  std::uint8_t answer[64] = {};
  CHECK_STATUS(rig.service().receive(nullptr, 16u, nullptr, 0u, answer, sizeof answer, answered, descriptor),
               Status::malformed);
  const Kind never_sent[] = {Kind::ready, Kind::reply, Kind::exported_image};
  for (const Kind kind : never_sent) {
      const int carried = fresh_descriptor();
      CHECK_STATUS(rig.send(kind, 3u, nullptr, 0u, &carried, 1u), Status::malformed);
      CHECK(!is_open(carried));
      CHECK_EQ(rig.answer_length(), 0u);
  }
  CHECK_EQ(rig.resources().live_descriptors(), 0u);
  const int none = -1;
  CHECK_STATUS(rig.service().receive(garbage, sizeof garbage, &none, 1u, answer, sizeof answer, answered,
                                     descriptor),
               Status::malformed); }

TEST(renderer_service_receive, descriptors_on_a_kind_that_takes_none_are_closed)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  std::uint8_t body[av::ContextMessage::encoded_size] = {};
  CHECK_STATUS(av::ContextMessage(context_id).encode(body, sizeof body), Status::ok);
  const Kind kinds[] = {Kind::create_context, Kind::destroy_context, Kind::blob_retired, Kind::guest_blob,
                        Kind::export_image, Kind::shared_released};
  for (const Kind kind : kinds) {
      const int carried[2] = {fresh_descriptor(), fresh_descriptor()};
      CHECK_STATUS(rig.send(kind, 4u, body, sizeof body, carried, 2u), Status::malformed);
      CHECK(!is_open(carried[0]));
      CHECK(!is_open(carried[1]));
      CHECK_EQ(rig.answer_length(), 0u);
  }
  CHECK_EQ(rig.resources().live_descriptors(), 0u);
  CHECK_EQ(av::RendererServiceProbe::plane(rig.service()).live(), 0u); }

TEST(renderer_service_region, maps_a_region_and_refuses_one_it_cannot_map_or_already_holds)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  rig.ram()[0x123] = 0x5Au;
  CHECK_STATUS(rig.region(), Status::ok);
  CHECK_EQ(rig.memory().count(), 1u);
  CHECK_EQ(rig.resources().live_mappings(), 1u);
  CHECK_EQ(rig.resources().live_descriptors(), 1u);
  const auto *const seen = static_cast<const std::uint8_t *>(
      rig.memory().resolve(std::uint64_t{region_number} << av::GuestMemoryTable::region_shift | 0x123u, 1u));
  CHECK(seen == rig.ram() + 0x123);
  CHECK_EQ(rig.answer_length(), 0u);
  CHECK_STATUS(rig.region(), Status::conflict);
  CHECK_EQ(rig.resources().live_mappings(), 1u);
  CHECK_EQ(rig.resources().live_descriptors(), 1u);
  const int stranger = fresh_descriptor();
  std::uint8_t body[av::RegionMessage::encoded_size] = {};
  CHECK_STATUS(av::RegionMessage(5u, 0x1000u).encode(body, sizeof body), Status::ok);
  CHECK_STATUS(rig.send(Kind::region, 2u, body, sizeof body, &stranger, 1u), Status::no_memory);
  CHECK(!is_open(stranger));
  CHECK_EQ(rig.resources().live_descriptors(), 1u);
  CHECK_STATUS(rig.send(Kind::region, 2u, body, sizeof body), Status::malformed);
  const int two[2] = {fresh_descriptor(), fresh_descriptor()};
  CHECK_STATUS(rig.send(Kind::region, 2u, body, sizeof body, two, 2u), Status::malformed);
  CHECK(!is_open(two[0]));
  CHECK(!is_open(two[1]));
  CHECK_EQ(rig.resources().live_descriptors(), 1u);
  CHECK_EQ(rig.memory().count(), 1u);
  CHECK(rig.service().invariants_hold()); }

TEST(renderer_service_guest_blob, mirrors_the_blob_over_the_bytes_its_extents_name)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  CHECK_STATUS(rig.region(), Status::ok);
  rig.ram()[0x1000] = 0xC3u;
  CHECK_STATUS(rig.guest_blob(21u, 0x1000u, 0x2000u), Status::ok);
  av::BlobTable &blobs = RendererServiceProbe::blobs(rig.service());
  CHECK(blobs.contains(21u));
  av::BlobWindow pages;
  CHECK_STATUS(blobs.backing(21u, pages), Status::ok);
  CHECK_EQ(pages.length(), 0x2000u);
  std::uint64_t bytes = 0u;
  CHECK(pages.extent(0u, bytes) == rig.ram() + 0x1000);
  std::uint8_t first = 0u;
  CHECK_STATUS(pages.read(0u, &first, 1u), Status::ok);
  CHECK_EQ(first, 0xC3u);
  CHECK_EQ(rig.answer_length(), 0u);
  CHECK_STATUS(rig.guest_blob(21u, 0x1000u, 0x1000u), Status::conflict);
  CHECK_STATUS(rig.guest_blob(22u, region_size - 0x1000u, 0x2000u), Status::not_found);
  CHECK(!blobs.contains(22u));
  CHECK(rig.service().invariants_hold()); }

TEST(renderer_service_guest_blob, refuses_an_extent_longer_than_a_memory_entry_carries)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  CHECK_STATUS(rig.region(), Status::ok);
  constexpr std::uint64_t past = 0x100000000u;
  const av::RendererExtent   extent(region_number, 0u, past);
  const av::GuestBlobMessage message(23u, past, &extent, 1u);
  std::uint8_t               body[64] = {};
  CHECK_STATUS(message.encode(body, sizeof body), Status::ok);
  CHECK_STATUS(rig.send(Kind::guest_blob, 3u, body, static_cast<std::uint32_t>(message.encoded_length())),
               Status::invalid_parameter);
  CHECK(!RendererServiceProbe::blobs(rig.service()).contains(23u));
  const av::RendererExtent   largest(region_number, 0u, past - 1u);
  const av::GuestBlobMessage fits(23u, past - 1u, &largest, 1u);
  CHECK_STATUS(fits.encode(body, sizeof body), Status::ok);
  CHECK_STATUS(rig.send(Kind::guest_blob, 3u, body, static_cast<std::uint32_t>(fits.encoded_length())),
               Status::not_found);
  CHECK_STATUS(rig.send(Kind::guest_blob, 3u, body, 8u), Status::malformed); }

TEST(renderer_service_allocate_memory, answers_the_librarys_memory_and_keeps_its_owner)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  rig.ops().set_memory_caching(AV_MEMORY_WRITE_COMBINED);
  const std::uint32_t key = av::HostBlobId::first + 2u;
  CHECK_STATUS(rig.allocate(context_id, key, 0x2000u, 7u), Status::ok);
  const av::AllocatedMemoryMessage answer = rig.allocated(7u);
  CHECK(answer.allocated());
  CHECK_EQ(answer.size(), 0x2000u);
  CHECK_EQ(answer.caching(), std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  const int handed = rig.answer_descriptor();
  CHECK(is_open(handed));
  const MockExecutorOps::MemoryRequest asked = rig.ops().last_memory_request();
  CHECK_EQ(asked.key, key);
  CHECK_EQ(asked.size, 0x2000u);
  CHECK_EQ(asked.request, 0x101u);
  CHECK(RendererServiceProbe::owners(rig.service()).owns(context_id, key));
  CHECK(!RendererServiceProbe::owners(rig.service()).owns(context_id + 1u, key));
  CHECK_EQ(rig.resources().live_descriptors(), 0u);
  CHECK_STATUS(rig.free_memory(key), Status::ok);
  CHECK(!is_open(handed));
  CHECK_EQ(rig.answer_length(), 0u);
  CHECK_EQ(rig.ops().memory_frees(), 1u);
  CHECK_EQ(rig.ops().held_memory(), 0u);
  CHECK(!RendererServiceProbe::owners(rig.service()).owns(context_id, key));
  CHECK(rig.service().invariants_hold()); }

TEST(renderer_service_allocate_memory, memory_it_cannot_hand_over_is_answered_as_none_and_given_back)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  rig.ops().refuse_memory_at(1u);
  CHECK_STATUS(rig.allocate(context_id, av::HostBlobId::first, 0x1000u), Status::ok);
  CHECK(!rig.allocated().allocated());
  CHECK_EQ(rig.answer_descriptor(), -1);
  CHECK_EQ(rig.ops().memory_frees(), 0u);
  const std::uint64_t not_blobs[] = {5u, av::HostBlobId::first - 1u,
                                     std::uint64_t{1} << 32 | av::HostBlobId::first};
  std::uint32_t frees = 0u;
  for (const std::uint64_t key : not_blobs) {
      CHECK_STATUS(rig.allocate(context_id, key, 0x1000u), Status::ok);
      CHECK(!rig.allocated().allocated());
      CHECK_EQ(rig.answer_descriptor(), -1);
      CHECK_EQ(rig.ops().memory_frees(), ++frees);
  }
  rig.ops().withhold_memory_descriptor_at(1u);
  CHECK_STATUS(rig.allocate(context_id, av::HostBlobId::first + 1u, 0x1000u), Status::ok);
  CHECK(!rig.allocated().allocated());
  CHECK_EQ(rig.ops().memory_frees(), frees + 1u);
  CHECK_EQ(rig.ops().held_memory(), 0u);
  CHECK_EQ(RendererServiceProbe::owners(rig.service()).live(), 0u);
  CHECK_EQ(rig.resources().live_descriptors(), 0u); }

TEST(renderer_service_allocate_memory, a_request_that_does_not_decode_or_an_answer_that_does_not_fit_is_refused)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  CHECK_STATUS(rig.allocate(0u, av::HostBlobId::first, 0x1000u), Status::malformed);
  CHECK_STATUS(rig.allocate(context_id, 0u, 0x1000u), Status::malformed);
  CHECK_STATUS(rig.allocate(context_id, av::HostBlobId::first, 0u), Status::malformed);
  std::uint8_t body[av::AllocateMemoryMessage::encoded_size] = {};
  CHECK_STATUS(av::AllocateMemoryMessage(context_id, 1u, av::HostBlobId::first, 0x1000u).encode(body, sizeof body),
               Status::ok);
  const int stray = fresh_descriptor();
  CHECK_STATUS(rig.send(Kind::allocate_memory, 5u, body, sizeof body, &stray, 1u), Status::malformed);
  CHECK(!is_open(stray));
  CHECK_EQ(rig.ops().memory_allocations(), 0u);
  CHECK_STATUS(rig.allocate(context_id, av::HostBlobId::first, 0x1000u, 5u, 8u), Status::too_large);
  CHECK_EQ(rig.answer_descriptor(), -1);
  CHECK_EQ(rig.resources().live_descriptors(), 0u); }

TEST(renderer_service_free_memory, frees_the_key_and_forgets_its_owner)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  CHECK_STATUS(rig.allocate(context_id, av::HostBlobId::first + 9u, 0x1000u), Status::ok);
  CHECK(RendererServiceProbe::owners(rig.service()).owns(context_id, av::HostBlobId::first + 9u));
  CHECK_STATUS(rig.free_memory(std::uint64_t{1} << 32), Status::ok);
  CHECK_EQ(rig.ops().held_memory(), 1u);
  CHECK_STATUS(rig.free_memory(av::HostBlobId::first + 9u), Status::ok);
  CHECK_EQ(rig.ops().held_memory(), 0u);
  CHECK(!RendererServiceProbe::owners(rig.service()).owns(context_id, av::HostBlobId::first + 9u));
  std::uint8_t body[av::FreeMemoryMessage::encoded_size] = {};
  CHECK_STATUS(rig.send(Kind::free_memory, 6u, body, sizeof body), Status::malformed);
  CHECK_STATUS(rig.send(Kind::free_memory, 6u, body, 4u), Status::malformed);
  CHECK_EQ(rig.answer_length(), 0u); }

TEST(renderer_service_blob_retired, retires_a_blob_and_refuses_one_it_does_not_hold)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  CHECK_STATUS(rig.region(), Status::ok);
  CHECK_STATUS(rig.guest_blob(21u, 0u, 0x1000u), Status::ok);
  std::uint8_t body[av::BlobRetiredMessage::encoded_size] = {};
  CHECK_STATUS(av::BlobRetiredMessage(21u).encode(body, sizeof body), Status::ok);
  CHECK_STATUS(rig.send(Kind::blob_retired, 4u, body, sizeof body), Status::ok);
  CHECK(!RendererServiceProbe::blobs(rig.service()).contains(21u));
  CHECK_STATUS(rig.send(Kind::blob_retired, 4u, body, sizeof body), Status::not_found);
  CHECK_STATUS(rig.send(Kind::blob_retired, 4u, body, 3u), Status::malformed);
  CHECK_EQ(rig.answer_length(), 0u); }

TEST(renderer_service_context, answers_create_and_destroy_with_the_planes_outcome)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  std::uint64_t sequence = 0u;
  CHECK_STATUS(rig.context(Kind::create_context, context_id, 40u), Status::ok);
  CHECK_STATUS(rig.reply(sequence).status(), Status::ok);
  CHECK_EQ(sequence, 40u);
  CHECK_EQ(rig.reply(sequence).sequence(), 40u);
  CHECK_EQ(RendererServiceProbe::plane(rig.service()).live(), 1u);
  CHECK_STATUS(rig.context(Kind::create_context, context_id, 41u), Status::ok);
  CHECK_STATUS(rig.reply(sequence).status(), Status::conflict);
  CHECK_EQ(sequence, 41u);
  CHECK_STATUS(rig.context(Kind::destroy_context, context_id, 42u), Status::ok);
  CHECK_STATUS(rig.reply(sequence).status(), Status::ok);
  CHECK_EQ(RendererServiceProbe::plane(rig.service()).live(), 0u);
  CHECK_STATUS(rig.context(Kind::destroy_context, context_id, 43u), Status::ok);
  CHECK_STATUS(rig.reply(sequence).status(), Status::not_found);
  CHECK_EQ(sequence, 43u);
  std::uint8_t zero[av::ContextMessage::encoded_size] = {};
  CHECK_STATUS(rig.send(Kind::create_context, 44u, zero, sizeof zero), Status::malformed);
  CHECK_EQ(rig.answer_length(), 0u);
  CHECK(rig.service().invariants_hold()); }

TEST(renderer_service_context, an_answer_that_does_not_fit_is_too_large)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  std::uint8_t datagram[RendererMessageHeader::encoded_size + av::ContextMessage::encoded_size] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::create_context, av::ContextMessage::encoded_size, 5u)
                   .encode(datagram, sizeof datagram),
               Status::ok);
  CHECK_STATUS(av::ContextMessage(context_id).encode(datagram + RendererMessageHeader::encoded_size, 4u), Status::ok);
  std::size_t  answered = 0u;
  int          descriptor = -1;
  std::uint8_t small[RendererMessageHeader::encoded_size] = {};
  CHECK_STATUS(rig.service().receive(datagram, sizeof datagram, nullptr, 0u, small, sizeof small, answered, descriptor),
               Status::too_large);
  CHECK_EQ(answered, 0u);
  CHECK_STATUS(rig.service().receive(datagram, sizeof datagram, nullptr, 0u, nullptr, 64u, answered, descriptor),
               Status::too_large);
  CHECK_EQ(RendererServiceProbe::plane(rig.service()).live(), 1u); }

TEST(renderer_service_bind, binds_the_context_to_its_doorbell_and_leaves_no_offer)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  CHECK_STATUS(rig.region(), Status::ok);
  rig.rings();
  CHECK_STATUS(rig.context(Kind::create_context, context_id, 1u), Status::ok);
  const int bell = fresh_descriptor();
  CHECK_STATUS(rig.bind(context_id, doorbell, bell, 7u), Status::ok);
  std::uint64_t sequence = 0u;
  CHECK_STATUS(rig.reply(sequence).status(), Status::ok);
  CHECK_EQ(sequence, 7u);
  CHECK(RendererServiceProbe::plane(rig.service()).holds_doorbell(doorbell));
  CHECK_EQ(rig.host().live_doorbells(), 1u);
  int left = -1;
  CHECK_STATUS(rig.doorbells().take(doorbell, left), Status::not_found);
  CHECK(!is_open(bell));
  CHECK_EQ(rig.resources().live_descriptors(), 1u);
  CHECK(rig.service().invariants_hold()); }

TEST(renderer_service_bind, a_refused_bind_leaves_no_doorbell_offered)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  CHECK_STATUS(rig.region(), Status::ok);
  rig.rings();
  const int bell = fresh_descriptor();
  CHECK_STATUS(rig.bind(99u, doorbell, bell, 8u), Status::ok);
  std::uint64_t sequence = 0u;
  CHECK_STATUS(rig.reply(sequence).status(), Status::invalid_parameter);
  CHECK_EQ(sequence, 8u);
  int left = -1;
  CHECK_STATUS(rig.doorbells().take(doorbell, left), Status::not_found);
  CHECK(!is_open(bell));
  CHECK_EQ(rig.host().live_doorbells(), 0u);
  CHECK_EQ(rig.resources().live_descriptors(), 1u); }

TEST(renderer_service_bind, a_doorbell_that_conflicts_is_refused_and_closed)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  CHECK_STATUS(rig.region(), Status::ok);
  rig.rings();
  CHECK_STATUS(rig.context(Kind::create_context, context_id, 1u), Status::ok);
  const int standing = fresh_descriptor();
  rig.resources().adopt(&standing, 1u);
  CHECK_STATUS(rig.doorbells().offer(doorbell, standing), Status::ok);
  const int bell = fresh_descriptor();
  CHECK_STATUS(rig.bind(context_id, doorbell, bell, 9u), Status::ok);
  std::uint64_t sequence = 0u;
  CHECK_STATUS(rig.reply(sequence).status(), Status::conflict);
  CHECK(!is_open(bell));
  CHECK(is_open(standing));
  CHECK(!RendererServiceProbe::plane(rig.service()).holds_doorbell(doorbell));
  CHECK_EQ(rig.host().live_doorbells(), 0u);
  rig.service().shut_down();
  CHECK(!is_open(standing));
  CHECK_EQ(rig.resources().live_descriptors(), 1u); }

TEST(renderer_service_bind, a_bind_without_a_doorbell_offers_none)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  CHECK_STATUS(rig.region(), Status::ok);
  rig.rings();
  CHECK_STATUS(rig.context(Kind::create_context, context_id, 1u), Status::ok);
  const av::ContextSetup setup(av::ProtocolVersion(1u, 0u), av::SubApi::vulkan, ring, 12u, fence, doorbell, 0u);
  std::uint8_t           setup_bytes[av::ContextSetup::encoded_size] = {};
  CHECK_STATUS(setup.encode(setup_bytes, sizeof setup_bytes), Status::ok);
  const av::BindContextMessage message(context_id, setup_bytes, static_cast<std::uint32_t>(setup.size()));
  std::uint8_t                 body[128] = {};
  CHECK_STATUS(message.encode(body, sizeof body), Status::ok);
  CHECK_STATUS(rig.send(Kind::bind_context, 4u, body, static_cast<std::uint32_t>(message.encoded_length())), Status::ok);
  std::uint64_t sequence = 0u;
  CHECK_STATUS(rig.reply(sequence).status(), Status::ok);
  CHECK_EQ(sequence, 4u);
  int left = -1;
  CHECK_STATUS(rig.doorbells().take(doorbell, left), Status::not_found);
  CHECK_EQ(rig.resources().live_descriptors(), 1u); }

TEST(renderer_service_bind, a_bind_that_does_not_decode_is_malformed_and_closed)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  std::uint8_t garbage_setup[av::ContextSetup::base_size] = {};
  std::memset(garbage_setup, 0x11, sizeof garbage_setup);
  const av::BindContextMessage message(context_id, garbage_setup, sizeof garbage_setup, doorbell);
  std::uint8_t                 body[128] = {};
  CHECK_STATUS(message.encode(body, sizeof body), Status::ok);
  const int bell = fresh_descriptor();
  CHECK_STATUS(rig.send(Kind::bind_context, 3u, body, static_cast<std::uint32_t>(message.encoded_length()), &bell, 1u),
               Status::malformed);
  CHECK(!is_open(bell));
  CHECK_STATUS(rig.send(Kind::bind_context, 3u, body, static_cast<std::uint32_t>(message.encoded_length())),
               Status::malformed);
  CHECK_EQ(rig.answer_length(), 0u);
  CHECK_EQ(rig.resources().live_descriptors(), 0u); }

TEST(renderer_service_interrupt_lines, takes_a_notifier_per_present_line_and_interrupts_off_drops_them)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  std::uint8_t body[av::InterruptLinesMessage::encoded_size] = {};
  CHECK_STATUS(av::InterruptLinesMessage(3u, 0b101u).encode(body, sizeof body), Status::ok);
  const int lines[2] = {fresh_descriptor(), fresh_descriptor()};
  CHECK_STATUS(rig.send(Kind::interrupt_lines, 2u, body, sizeof body, lines, 2u), Status::ok);
  CHECK_EQ(rig.resources().interrupt_lines(), 2u);
  CHECK(rig.host().fence_interrupts_connected());
  CHECK(is_open(lines[0]));
  CHECK(is_open(lines[1]));
  CHECK_EQ(rig.answer_length(), 0u);
  const int one = fresh_descriptor();
  CHECK_STATUS(rig.send(Kind::interrupt_lines, 2u, body, sizeof body, &one, 1u), Status::malformed);
  CHECK(!is_open(one));
  CHECK_EQ(rig.resources().interrupt_lines(), 2u);
  CHECK_STATUS(rig.send(Kind::interrupts_off, 3u, nullptr, 0u), Status::ok);
  CHECK_EQ(rig.resources().interrupt_lines(), 0u);
  CHECK(!rig.host().fence_interrupts_connected());
  CHECK(!is_open(lines[0]));
  CHECK_EQ(rig.resources().live_descriptors(), 0u); }

TEST(renderer_service_export_image, sends_a_slots_dma_buf_only_when_the_image_changed)
{ Rig rig;
  rig.ops().editable().export_image = &export_published;
  CHECK_STATUS(rig.hello(), Status::ok);
  const int image_fd = fresh_descriptor();
  published = Published{true, AvExportedImage{image_fd, 1u, 64u, 32u, 256u, 0u, 0x34325258u, 0u, 7u}};
  std::uint8_t body[av::ExportImageMessage::encoded_size] = {};
  CHECK_STATUS(av::ExportImageMessage(0x77u).encode(body, sizeof body), Status::ok);
  const auto exported = [&rig](av::ExportedImageMessage &message, std::uint64_t &sequence) {
      RendererMessageHeader header;
      sequence = 0u;
      if (header.decode(rig.answer(), rig.answer_length()) != Status::ok || header.kind() != Kind::exported_image)
          return false;
      sequence = header.sequence();
      return message.decode(rig.answer() + RendererMessageHeader::encoded_size, header.body_length(),
                            rig.answer_descriptor() >= 0 ? 1u : 0u) == Status::ok;
  };
  av::ExportedImageMessage message;
  std::uint64_t            sequence = 0u;
  CHECK_STATUS(rig.send(Kind::export_image, 60u, body, sizeof body), Status::ok);
  CHECK(exported(message, sequence));
  CHECK_EQ(sequence, 60u);
  CHECK(message.found());
  CHECK(message.fresh());
  CHECK_EQ(rig.answer_descriptor(), image_fd);
  CHECK_EQ(message.slot(), 1u);
  CHECK_EQ(message.width(), 64u);
  CHECK_EQ(message.height(), 32u);
  CHECK_EQ(message.stride(), 256u);
  CHECK_EQ(message.fourcc(), 0x34325258u);
  CHECK_EQ(message.modifier(), 7u);
  CHECK_STATUS(rig.send(Kind::export_image, 61u, body, sizeof body), Status::ok);
  CHECK(exported(message, sequence));
  CHECK(message.found());
  CHECK(!message.fresh());
  CHECK_EQ(rig.answer_descriptor(), -1);
  published.image.height = 48u;
  CHECK_STATUS(rig.send(Kind::export_image, 62u, body, sizeof body), Status::ok);
  CHECK(exported(message, sequence));
  CHECK(message.fresh());
  CHECK_EQ(message.height(), 48u);
  CHECK_EQ(rig.answer_descriptor(), image_fd);
  const auto changed = [&](void (*change)(AvExportedImage &)) {
      change(published.image);
      std::uint64_t at = 0u;
      av::ExportedImageMessage again;
      return rig.send(Kind::export_image, 70u, body, sizeof body) == Status::ok && exported(again, at) &&
             again.fresh() && rig.answer_descriptor() == image_fd;
  };
  CHECK(changed([](AvExportedImage &image) { image.width = 65u; }));
  CHECK(changed([](AvExportedImage &image) { image.stride = 512u; }));
  CHECK(changed([](AvExportedImage &image) { image.offset = 32u; }));
  CHECK(changed([](AvExportedImage &image) { image.fourcc = 0x34324241u; }));
  CHECK(changed([](AvExportedImage &image) { image.modifier = 9u; }));
  CHECK_STATUS(rig.send(Kind::export_image, 71u, body, sizeof body), Status::ok);
  CHECK(exported(message, sequence));
  CHECK(!message.fresh());
  published.image.fd = -1;
  CHECK_STATUS(rig.send(Kind::export_image, 72u, body, sizeof body), Status::ok);
  CHECK(exported(message, sequence));
  CHECK(!message.found());
  published.image.fd = image_fd;
  published.image.slot = av::ExportedImageMessage::slots;
  CHECK_STATUS(rig.send(Kind::export_image, 63u, body, sizeof body), Status::ok);
  CHECK(exported(message, sequence));
  CHECK(!message.found());
  CHECK_EQ(rig.answer_descriptor(), -1);
  published.found = false;
  CHECK_STATUS(rig.send(Kind::export_image, 64u, body, sizeof body), Status::ok);
  CHECK(exported(message, sequence));
  CHECK(!message.found());
  CHECK_STATUS(rig.send(Kind::export_image, 65u, body, 7u), Status::malformed);
  CHECK_EQ(rig.answer_length(), 0u);
  CHECK(is_open(image_fd));
  ::close(image_fd); }

TEST(renderer_service_shut_down, leaves_no_context_blob_line_or_offer)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  CHECK_STATUS(rig.allocate(context_id, av::HostBlobId::first, 0x1000u), Status::ok);
  CHECK_STATUS(rig.region(), Status::ok);
  rig.rings();
  CHECK_STATUS(rig.context(Kind::create_context, context_id, 1u), Status::ok);
  CHECK_STATUS(rig.bind(context_id, doorbell, fresh_descriptor(), 2u), Status::ok);
  std::uint8_t body[av::InterruptLinesMessage::encoded_size] = {};
  CHECK_STATUS(av::InterruptLinesMessage(1u, 1u).encode(body, sizeof body), Status::ok);
  const int line = fresh_descriptor();
  CHECK_STATUS(rig.send(Kind::interrupt_lines, 3u, body, sizeof body, &line, 1u), Status::ok);
  const int offered = fresh_descriptor();
  rig.resources().adopt(&offered, 1u);
  CHECK_STATUS(rig.doorbells().offer(9u, offered), Status::ok);
  rig.service().shut_down();
  CHECK_EQ(RendererServiceProbe::plane(rig.service()).live(), 0u);
  CHECK_EQ(RendererServiceProbe::blobs(rig.service()).live(), 0u);
  CHECK_EQ(RendererServiceProbe::owners(rig.service()).live(), 0u);
  CHECK_EQ(rig.resources().interrupt_lines(), 0u);
  CHECK(!rig.host().fence_interrupts_connected());
  CHECK(!is_open(offered));
  CHECK(!is_open(line));
  CHECK_EQ(rig.host().live_doorbells(), 0u);
  CHECK_EQ(rig.host().live_threads(), 0u);
  CHECK_EQ(rig.resources().live_descriptors(), 1u);
  CHECK(rig.service().invariants_hold()); }

TEST(renderer_service_invariants, a_context_without_a_hello_breaks_them)
{ Rig rig;
  CHECK_STATUS(rig.hello(), Status::ok);
  CHECK_STATUS(rig.context(Kind::create_context, context_id, 1u), Status::ok);
  CHECK(rig.service().invariants_hold());
  RendererServiceProbe::forget_hello(rig.service());
  CHECK(!rig.service().invariants_hold());
  CHECK_STATUS(rig.context(Kind::destroy_context, context_id, 2u), Status::invalid_state);
  rig.service().shut_down();
  CHECK(rig.service().invariants_hold()); }
