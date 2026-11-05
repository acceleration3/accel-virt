#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "guest/transport/shared_object_call.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::AllocationDescriptor;
using av::AllocationKind;
using av::DdiKmt;
using av::FullStackRig;
using av::GuestConnection;
using av::KmtTransport;
using av::ScanoutTarget;
using av::SharedObjectCall;
using av::SharedObjectKind;
using av::SharedObjectRecord;
using av::Status;
using av::SubApi;
using av::SurfaceLayout;

namespace {

constexpr std::size_t   staging_bytes = 1024u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::uint8_t  ring_order = 12u;
constexpr std::uint64_t object_bytes = 4u * 4096u;
constexpr std::uint32_t memory_kind = SharedObjectKind::memory;
constexpr std::uint64_t published_handle = 0x7001u;

class GuestContext {
public:
    explicit GuestContext(DdiKmt &kmt) : connection_(kmt),
        transport_(connection_, SubApi::vulkan, ring_order, reply_bytes, staging_, sizeof staging_) {}
    ~GuestContext() { close(); }
    GuestContext(const GuestContext &) = delete;
    GuestContext &operator=(const GuestContext &) = delete;

    [[nodiscard]] Status open(std::uint32_t adapter)
    {
        const Status opened = connection_.open(adapter);
        if (opened != Status::ok) return opened;
        const Status ready = transport_.initialize();
        if (ready != Status::ok) return ready;
        live_ = true;
        return Status::ok;
    }
    void close_context()
    {
        if (!live_) return;
        live_ = false;
        transport_.teardown();
    }
    void close()
    {
        close_context();
        if (connection_.device() == 0u) return;
        connection_.close();
    }

    std::uint32_t  device() const { return connection_.device(); }
    KmtTransport  &transport() { return transport_; }

private:
    GuestConnection connection_;
    KmtTransport    transport_;
    std::uint8_t    staging_[staging_bytes] = {};
    bool            live_ = false;
};

std::uint64_t minted_carrier(DdiKmt &kmt, std::uint32_t device, std::uint32_t &carrier)
{
    std::uint8_t bytes[AllocationDescriptor::encoded_size + SharedObjectRecord::encoded_size] = {};
    const AllocationDescriptor descriptor(AllocationKind::client, true, SurfaceLayout(),
                                          SharedObjectRecord::page_size, ScanoutTarget());
    CHECK_STATUS(descriptor.encode(bytes, AllocationDescriptor::encoded_size), Status::ok);
    const SharedObjectRecord asking(0u, SharedObjectKind(memory_kind), 0u, object_bytes);
    CHECK_STATUS(asking.encode(bytes + AllocationDescriptor::encoded_size, SharedObjectRecord::encoded_size),
                 Status::ok);
    CHECK_STATUS(kmt.create_shared_allocation(device, bytes, sizeof bytes, carrier), Status::ok);
    SharedObjectRecord minted;
    CHECK_STATUS(minted.decode(bytes + AllocationDescriptor::encoded_size, SharedObjectRecord::encoded_size),
                 Status::ok);
    CHECK(minted.assigned());
    return minted.object_id();
}

std::uint64_t crossed_id(DdiKmt &kmt, std::uint32_t opener, std::uint64_t handle, std::uint32_t &resource)
{
    std::uint8_t  carried[128] = {};
    std::uint32_t written = 0u;
    CHECK_STATUS(kmt.open_shared_object(opener, handle, resource, carried, sizeof carried, written),
                 Status::ok);
    CHECK_EQ(written, AllocationDescriptor::encoded_size + SharedObjectRecord::encoded_size);
    SharedObjectRecord record;
    CHECK_STATUS(record.decode(carried + AllocationDescriptor::encoded_size, SharedObjectRecord::encoded_size),
                 Status::ok);
    CHECK(record.assigned());
    return record.object_id();
}

}

TEST(fullstack_shared_objects, a_second_context_opens_what_the_first_published_and_outlives_it)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  DdiKmt       &kmt = rig.kmt();
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);

  GuestContext publisher(kmt);
  GuestContext opener(kmt);
  CHECK_STATUS(publisher.open(adapter), Status::ok);
  CHECK_STATUS(opener.open(adapter), Status::ok);
  CHECK_EQ(rig.device().device().contexts(), 2u);
  CHECK_EQ(rig.device().executor().shared_objects(), 0u);

  std::uint32_t      carrier = 0u;
  const std::uint64_t object_id = minted_carrier(kmt, publisher.device(), carrier);
  CHECK(object_id != 0u);

  SharedObjectCall publish(publisher.transport());
  CHECK_STATUS(publish.share(object_id, memory_kind, object_bytes, published_handle), Status::ok);
  CHECK_EQ(rig.device().executor().shared_objects(), 1u);

  std::uint64_t handle = 0u;
  CHECK_STATUS(kmt.share_object(publisher.device(), carrier, handle), Status::ok);
  CHECK(handle != 0u);
  std::uint32_t opened_resource = 0u;
  CHECK_EQ(crossed_id(kmt, opener.device(), handle, opened_resource), object_id);
  CHECK(opened_resource != 0u);

  SharedObjectCall open(opener.transport());
  std::uint64_t    opened_handle = 0u;
  CHECK_STATUS(open.open(object_id, memory_kind, object_bytes, 0u, opened_handle), Status::ok);
  CHECK(opened_handle != 0u);
  CHECK(opened_handle != published_handle);
  CHECK_EQ(rig.device().executor().shared_objects(), 1u);
  CHECK_EQ(rig.device().executor().opened_objects(), 1u);
  CHECK_EQ(rig.device().executor().live_opens(), 1u);

  publisher.close_context();
  CHECK_EQ(rig.device().device().contexts(), 1u);
  CHECK_EQ(rig.device().executor().live_opens(), 1u);

  CHECK_STATUS(open.release(object_id, memory_kind, object_bytes, 0u), Status::ok);
  CHECK_EQ(rig.device().executor().live_opens(), 0u);
  CHECK_EQ(rig.device().executor().closed_objects(), 1u);

  CHECK_STATUS(kmt.destroy_shared_allocation(opener.device(), opened_resource), Status::ok);
  CHECK_STATUS(kmt.close_shared_object(handle), Status::ok);
  CHECK_STATUS(kmt.destroy_shared_allocation(publisher.device(), carrier), Status::ok);
  publisher.close();
  opener.close();
  CHECK_EQ(rig.device().device().contexts(), 0u);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(fullstack_shared_objects, ids_a_context_never_published_are_refused_and_nothing_else_is)
{ FullStackRig rig;
  CHECK_EQ(rig.started(), 0);
  DdiKmt       &kmt = rig.kmt();
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  GuestContext publisher(kmt);
  GuestContext hostile(kmt);
  CHECK_STATUS(publisher.open(adapter), Status::ok);
  CHECK_STATUS(hostile.open(adapter), Status::ok);

  std::uint32_t       carrier = 0u;
  const std::uint64_t object_id = minted_carrier(kmt, publisher.device(), carrier);
  SharedObjectCall    publish(publisher.transport());
  CHECK_STATUS(publish.share(object_id, memory_kind, object_bytes, published_handle), Status::ok);
  CHECK_EQ(rig.device().executor().shared_objects(), 1u);

  SharedObjectCall attack(hostile.transport());
  std::uint64_t    got = 9u;
  CHECK_STATUS(attack.open(object_id + 0x1000u, memory_kind, object_bytes, 0u, got),
               Status::invalid_parameter);
  CHECK_EQ(attack.device_status(), 2u);
  CHECK_EQ(got, 0u);
  CHECK_STATUS(attack.open(object_id, SharedObjectKind::monitored_fence, 0u, 0u, got),
               Status::invalid_parameter);
  CHECK_EQ(attack.device_status(), 3u);
  CHECK_STATUS(attack.open(object_id, memory_kind, object_bytes * 2u, 0u, got), Status::invalid_parameter);
  CHECK_EQ(attack.device_status(), 3u);
  CHECK_STATUS(attack.open(object_id, 4u, object_bytes, 0u, got), Status::invalid_parameter);
  CHECK_EQ(attack.device_status(), 0u);
  CHECK_EQ(got, 0u);
  CHECK_STATUS(attack.share(object_id, memory_kind, object_bytes * 2u, published_handle),
               Status::invalid_parameter);
  CHECK_EQ(attack.device_status(), 6u);

  CHECK_EQ(rig.device().executor().shared_objects(), 1u);
  CHECK_EQ(rig.device().executor().live_opens(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 2u);
  CHECK_EQ(rig.device().executor().opened_objects(), 0u);

  CHECK_STATUS(attack.open(object_id, memory_kind, object_bytes, 0u, got), Status::ok);
  CHECK(got != 0u);
  CHECK_EQ(rig.device().executor().live_opens(), 1u);
  std::uint64_t again = 9u;
  CHECK_STATUS(attack.open(object_id, memory_kind, object_bytes, 0u, again), Status::invalid_parameter);
  CHECK_EQ(attack.device_status(), 6u);
  CHECK_EQ(again, 0u);
  CHECK_EQ(rig.device().executor().live_opens(), 1u);
  CHECK_EQ(rig.device().executor().opened_objects(), 1u);

  CHECK_STATUS(attack.release(object_id, memory_kind, object_bytes, 0u), Status::ok);
  CHECK_EQ(rig.device().executor().live_opens(), 0u);
  publisher.close_context();
  CHECK_STATUS(kmt.destroy_shared_allocation(publisher.device(), carrier), Status::ok);
  publisher.close();
  hostile.close();
  CHECK_EQ(kmt.open_objects(), 0u); }
