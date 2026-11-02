#include "miniport/ddi/dxgkarg_doorbell_records.hpp"
#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "guest/transport/transport_ring.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/loopback/loopback_device.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sched.h>

namespace av {
struct TransportRingProbe {
    static std::uint8_t *bytes(TransportRing &ring) { return ring.ring_; }
};

struct KmtTransportProbe {
    static std::uint8_t *ring(KmtTransport &transport) { return TransportRingProbe::bytes(transport.ring_); }
    static void          forget_fault(KmtTransport &transport) { transport.faulted_ = false; }
};

}

using av::DdiKmt;
using av::KmtTransportProbe;
using av::DoorbellStatus;
using av::FullStackRig;
using av::GuestConnection;
using av::KmtTransport;
using av::Status;
using av::SubApi;
using av::TransportRing;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;

namespace {

constexpr std::size_t   staging_bytes = 1024u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::uint32_t echo_command = 0x10001u;
constexpr std::uint64_t echo_payload = 0x0123456789ABCDEFull;
constexpr std::uint8_t  ring_order = 12u;

class Submitting {
public:
    Submitting() : connection_(rig_.kmt()), transport_(connection_, SubApi::vulkan, ring_order, reply_bytes,
                                                       staging_, sizeof staging_)
    {
        rig_.kmt().allow_hardware_scheduling();
        CHECK_STATUS(connection_.open(DdiKmt::adapter_handle), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
    }
    ~Submitting()
    {
        transport_.teardown();
        connection_.close();
    }
    Submitting(const Submitting &) = delete;
    Submitting &operator=(const Submitting &) = delete;

    FullStackRig &rig() { return rig_; }
    KmtTransport &transport() { return transport_; }
    DdiKmt       &kmt() { return rig_.kmt(); }
    std::uint32_t doorbell() { return rig_.kmt().doorbell_handle(0u); }
    std::uint32_t device_stores() { return rig_.device().doorbells().stores(); }

    bool echoes(std::uint64_t payload)
    {
        WireWriter request = transport_.request_writer();
        request.write_u64(payload);
        WireReader reply(nullptr, 0u);
        const bool answered = transport_.send_sync(echo_command, request, reply) == TransportResult::ok;
        static_cast<void>(rig_.kmt().fabric().deliver());
        return answered && reply.remaining() == sizeof payload && reply.read_u64() == payload;
    }

private:
    FullStackRig    rig_;
    GuestConnection connection_;
    std::uint8_t    staging_[staging_bytes] = {};
    KmtTransport    transport_;
};

}

TEST(fullstack_doorbell, a_user_mode_store_reaches_the_device_worker_with_no_kernel_submission)
{ Submitting submitting;
  CHECK(submitting.doorbell() != 0u);
  const std::uint32_t stores = submitting.device_stores();
  CHECK(submitting.echoes(echo_payload));
  CHECK_EQ(submitting.transport().submissions_through_doorbell(), 1u);
  CHECK_EQ(submitting.transport().submissions_through_kernel(), 0u);
  CHECK_EQ(submitting.device_stores(), stores + 1u);
  CHECK_EQ(submitting.kmt().fabric().delivered(), 1u);
  CHECK(submitting.kmt().fabric().invariants_hold());
  CHECK_EQ(submitting.kmt().fences().completed(), 0u);
  CHECK(submitting.echoes(echo_payload ^ 1u));
  CHECK_EQ(submitting.transport().submissions_through_doorbell(), 2u);
  CHECK_EQ(submitting.device_stores(), stores + 2u); }

TEST(fullstack_doorbell, a_store_to_a_disconnected_doorbell_reaches_nothing)
{ Submitting submitting;
  CHECK(submitting.echoes(echo_payload));
  const std::uint32_t stores = submitting.device_stores();
  const std::uint32_t handle = submitting.doorbell();
  CHECK_STATUS(submitting.kmt().disconnect_doorbell(handle, DoorbellStatus::disconnected_retry), Status::ok);

  volatile std::uint32_t *const page = static_cast<volatile std::uint32_t *>(submitting.kmt().doorbell_page(handle));
  CHECK(page != nullptr);
  *page = 1u;
  CHECK_EQ(submitting.kmt().fabric().deliver(), 0u);
  CHECK_EQ(submitting.kmt().fabric().dropped(), 1u);
  CHECK_EQ(submitting.device_stores(), stores);

  const std::uint64_t through_doorbell = submitting.transport().submissions_through_doorbell();
  CHECK(submitting.echoes(echo_payload ^ 2u));
  CHECK_EQ(submitting.transport().submissions_through_doorbell(), through_doorbell);
  CHECK_EQ(submitting.transport().submissions_through_kernel(), 1u);
  CHECK(submitting.device_stores() <= stores + 1u); }

TEST(fullstack_doorbell, no_record_is_executed_twice_or_lost_across_a_disconnect)
{ Submitting submitting;
  constexpr std::uint32_t each = 8u;
  std::uint32_t           returned = 0u;
  for (std::uint32_t batch = 0u; batch < each; batch++)
      returned += submitting.echoes(echo_payload ^ batch) ? 1u : 0u;
  CHECK_EQ(submitting.transport().submissions_through_doorbell(), each);

  CHECK_STATUS(submitting.kmt().disconnect_doorbell(submitting.doorbell(), DoorbellStatus::disconnected_retry),
               Status::ok);
  for (std::uint32_t batch = each; batch < 2u * each; batch++)
      returned += submitting.echoes(echo_payload ^ batch) ? 1u : 0u;
  CHECK_EQ(submitting.transport().submissions_through_kernel(), 1u);

  for (std::uint32_t batch = 2u * each; batch < 3u * each; batch++)
      returned += submitting.echoes(echo_payload ^ batch) ? 1u : 0u;
  CHECK_EQ(returned, 3u * each);
  CHECK_EQ(submitting.transport().submissions_through_doorbell() +
               submitting.transport().submissions_through_kernel(),
           3u * each);
  CHECK(submitting.transport().invariants_hold()); }

TEST(fullstack_doorbell, work_already_stored_for_completes_across_a_disconnect)
{ Submitting submitting;
  WireWriter request = submitting.transport().request_writer();
  request.write_u64(echo_payload);
  CHECK(submitting.transport().send_async(echo_command, request) == TransportResult::ok);
  CHECK_EQ(submitting.kmt().fabric().deliver(), 1u);
  CHECK_STATUS(submitting.kmt().disconnect_doorbell(submitting.doorbell(), DoorbellStatus::disconnected_abort),
               Status::ok);

  CHECK(submitting.echoes(echo_payload ^ 3u));
  CHECK_EQ(submitting.transport().submissions_through_kernel(), 1u);
  CHECK(submitting.doorbell() != 0u);
  CHECK(submitting.echoes(echo_payload ^ 4u));
  CHECK_EQ(submitting.transport().submissions_through_doorbell(), 2u);
  CHECK(submitting.kmt().fabric().invariants_hold()); }

namespace {
class Neighbours {
public:
    Neighbours() : connection_(rig_.kmt()),
                   doomed_(connection_, SubApi::vulkan, ring_order, reply_bytes, doomed_staging_,
                           sizeof doomed_staging_),
                   healthy_(connection_, SubApi::vulkan, ring_order, reply_bytes, healthy_staging_,
                            sizeof healthy_staging_)
    {
        rig_.kmt().allow_hardware_scheduling();
        CHECK_STATUS(connection_.open(DdiKmt::adapter_handle), Status::ok);
        CHECK_STATUS(doomed_.initialize(), Status::ok);
        CHECK_STATUS(healthy_.initialize(), Status::ok);
    }
    ~Neighbours()
    {
        doomed_.teardown();
        healthy_.teardown();
        connection_.close();
    }
    Neighbours(const Neighbours &) = delete;
    Neighbours &operator=(const Neighbours &) = delete;

    FullStackRig &rig() { return rig_; }
    DdiKmt       &kmt() { return rig_.kmt(); }
    KmtTransport &doomed() { return doomed_; }
    KmtTransport &healthy() { return healthy_; }
    std::uint32_t doomed_doorbell() { return rig_.kmt().doorbell_handle(0u); }

    void store(std::uint32_t doorbell, std::uint32_t value)
    {
        volatile std::uint32_t *const page = static_cast<volatile std::uint32_t *>(kmt().doorbell_page(doorbell));
        CHECK(page != nullptr);
        *page = value;
    }
    bool echoes(KmtTransport &transport, std::uint64_t payload)
    {
        WireWriter request = transport.request_writer();
        request.write_u64(payload);
        WireReader reply(nullptr, 0u);
        const bool answered = transport.send_sync(echo_command, request, reply) == TransportResult::ok;
        static_cast<void>(rig_.kmt().fabric().deliver());
        return answered && reply.remaining() == sizeof payload && reply.read_u64() == payload;
    }

private:
    FullStackRig    rig_;
    GuestConnection connection_;
    std::uint8_t    doomed_staging_[staging_bytes] = {};
    std::uint8_t    healthy_staging_[staging_bytes] = {};
    KmtTransport    doomed_;
    KmtTransport    healthy_;
};

constexpr std::size_t tail_offset = 8u;

bool faults_soon(Neighbours &neighbours)
{
    constexpr std::uint32_t passes = 20000u;
    for (std::uint32_t pass = 0u; pass < passes; pass++) {
        static_cast<void>(neighbours.kmt().fabric().deliver());
        neighbours.rig().pump().deliver();
        if (std::strstr(neighbours.rig().device().host().last_log(), "context faulted") != nullptr) return true;
        sched_yield();
    }
    return false;
}

}

TEST(fullstack_doorbell_hostile, a_store_of_any_value_is_a_wake_and_nothing_more)
{ Neighbours          neighbours;
  const std::uint32_t values[] = {0xFFFFFFFFu, 0x80000000u, 2u, 0xDEADBEEFu};
  for (const std::uint32_t value : values) {
      neighbours.store(neighbours.doomed_doorbell(), value);
      CHECK_EQ(neighbours.kmt().fabric().deliver(), 1u);
  }
  CHECK(neighbours.echoes(neighbours.doomed(), echo_payload));
  CHECK(neighbours.echoes(neighbours.healthy(), echo_payload ^ 1u));
  CHECK(!neighbours.doomed().faulted()); }

TEST(fullstack_doorbell_hostile, a_tail_past_the_ring_faults_only_that_context)
{ Neighbours neighbours;
  CHECK(neighbours.echoes(neighbours.doomed(), echo_payload));
  CHECK(neighbours.echoes(neighbours.healthy(), echo_payload));

  std::uint8_t *const  ring = KmtTransportProbe::ring(neighbours.doomed());
  const std::uint64_t  past = std::uint64_t{1} << 40u;
  std::memcpy(ring + tail_offset, &past, sizeof past);
  neighbours.store(neighbours.doomed_doorbell(), 1u);
  CHECK(faults_soon(neighbours));

  KmtTransportProbe::forget_fault(neighbours.doomed());
  CHECK(!neighbours.echoes(neighbours.doomed(), echo_payload ^ 2u));
  CHECK(neighbours.echoes(neighbours.healthy(), echo_payload ^ 3u));
  CHECK(!neighbours.healthy().faulted()); }

TEST(fullstack_doorbell_hostile, a_queued_fence_nobody_submitted_reaches_no_device)
{ Neighbours neighbours;
  CHECK(neighbours.echoes(neighbours.doomed(), echo_payload));
  std::uint64_t *const queued = neighbours.kmt().queued_fence(neighbours.doomed_doorbell());
  CHECK(queued != nullptr);
  const std::uint32_t stores = neighbours.rig().device().doorbells().stores();
  *queued = std::uint64_t{1} << 50u;
  CHECK_EQ(neighbours.kmt().fabric().deliver(), 0u);
  CHECK_EQ(neighbours.rig().device().doorbells().stores(), stores);

  CHECK(neighbours.echoes(neighbours.healthy(), echo_payload ^ 4u));
  CHECK(!neighbours.healthy().faulted()); }
