/* A context's ring, fence and reply become guest blobs the device can name, one
 * RESOURCE_CREATE_BLOB over the aperture pages VidMm handed the driver. An
 * allocation that is not resident, or not whole, has an incomplete page list
 * and is refused before anything is created; teardown unrefs exactly the blobs
 * it made. The device answers over a real control queue, as it does at
 * StartDevice. */
#include "core/rvk/context_blob_publisher.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstdint>
#include <cstring>
#include <optional>

using av::ContextBlobPublisher;
using av::ContextBlobPublisherProbe;
using av::FakeVirtqueueDevice;
using av::GpuResponse;
using av::GuestPageList;
using av::Irql;
using av::MappedVirtioFunction;
using av::Status;
using av::VirtioGpuDevice;
using av::Violation;

namespace av {

struct ContextBlobPublisherProbe {
    /* A held slot with an impossible blob count, and an id counter below its
     * base: the states the class never builds and its invariant catches. */
    static void break_count(ContextBlobPublisher &publisher)
    {
        publisher.used_[0] = true;
        publisher.counts_[0] = 0u;
    }
    static void break_next_id(ContextBlobPublisher &publisher) { publisher.next_id_ = 0u; }
    static void break_count_high(ContextBlobPublisher &publisher)
    {
        publisher.used_[0] = true;
        publisher.counts_[0] = ContextBlobPublisher::blobs_per_context + 1u;
    }
};

}  // namespace av

namespace {

constexpr std::uint16_t control_queue = 0u;
constexpr std::uint32_t create_blob_type = 0x010Cu;   /* VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB */
constexpr std::uint32_t unref_type       = 0x0102u;   /* VIRTIO_GPU_CMD_RESOURCE_UNREF */

/* A device that answers each control request from the stall between the
 * driver's polls, OK_NODATA by default, refusing on request. */
class Host {
public:
    explicit Host(MappedVirtioFunction &mapped) : queue_(mapped.queue_device(control_queue))
    { mapped.platform().on_stall(&Host::stall, this); }

    /* Refuse the request at 0-based `index`; -1 refuses none. */
    void refuse_at(int index) { refuse_at_ = index; }
    void answer_nothing() { silent_ = true; }
    /* Bytes too short to be a response header. */
    void answer_garbage() { garbage_ = true; }
    FakeVirtqueueDevice &queue() { return queue_; }
    std::uint32_t answered() const { return answered_; }

    /* How many control requests of `type` the device has been sent. */
    std::uint32_t count_of(std::uint32_t type) const
    {
        std::uint32_t seen = 0u;
        for (std::uint32_t i = 0u; i < queue_.requests(); i++)
            if (queue_.request_type(i) == type) seen++;
        return seen;
    }
    /* The resource id of request `index` (offset 24, past the header). */
    std::uint32_t resource_of(std::uint32_t index) const
    {
        std::uint32_t id = 0u;
        std::memcpy(&id, queue_.request(index) + 24u, 4u);
        return id;
    }

private:
    static void stall(void *context)
    {
        auto *const host = static_cast<Host *>(context);
        const std::uint32_t asked = host->queue_.pending_type();
        if (asked == 0u || host->silent_) return;
        if (host->garbage_) {
            const std::uint8_t rubbish[8] = {0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE};
            host->answered_++;
            static_cast<void>(host->queue_.respond_all(rubbish, sizeof rubbish));
            return;
        }
        GpuResponse answer;
        if (static_cast<int>(host->answered_) == host->refuse_at_)
            answer.set(GpuResponse::err_invalid_parameter);
        else
            answer.set(GpuResponse::ok_nodata);
        host->answered_++;
        static_cast<void>(host->queue_.respond_all(answer.bytes(),
                                                   static_cast<std::uint32_t>(answer.length())));
    }

    FakeVirtqueueDevice queue_;
    std::uint32_t       answered_ = 0u;
    int                 refuse_at_ = -1;
    bool                silent_ = false;
    bool                garbage_ = false;
};

class Fixture {
public:
    Status start()
    {
        const Status mapped = mapped_.map();
        if (mapped != Status::ok) return mapped;
        device_.emplace(mapped_.platform(), mapped_.mappings(), mapped_.layout());
        return device_->start(3u);
    }
    MappedVirtioFunction &mapped() { return mapped_; }
    VirtioGpuDevice      &device() { return *device_; }

private:
    MappedVirtioFunction           mapped_;
    std::optional<VirtioGpuDevice> device_;
};

/* Three page-aligned frames, so each list is complete: a lone page and a pair. */
constexpr std::uint64_t frames[] = {0x11111000ull, 0x22222000ull, 0x22223000ull};
GuestPageList whole() { return GuestPageList(frames, 3u); }

}  // namespace

TEST(context_blob_publisher, holds_nothing_until_a_context_publishes)
{ MappedVirtioFunction mapped;
  ContextBlobPublisher publisher(mapped.platform());
  CHECK(!publisher.holds(7u));
  CHECK(publisher.invariants_hold()); }

TEST(context_blob_publisher, three_allocations_become_three_distinct_named_blobs)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  ContextBlobPublisher publisher(fixture.mapped().platform());
  const GuestPageList blobs[] = {whole(), whole(), whole()};
  std::uint32_t ids[3] = {};
  CHECK_STATUS(publisher.publish(fixture.device(), 7u, blobs, 3u, ids), Status::ok);
  CHECK(publisher.holds(7u));
  CHECK_EQ(host.count_of(create_blob_type), 3u);
  /* Three distinct non-zero ids above the reserved range, and the device was
   * told each. */
  CHECK(ids[0] != 0u && ids[1] != 0u && ids[2] != 0u);
  CHECK(ids[0] != ids[1] && ids[1] != ids[2] && ids[0] != ids[2]);
  CHECK(ids[0] >= ContextBlobPublisher::first_resource_id);
  CHECK_EQ(host.resource_of(0u), ids[0]);
  CHECK(publisher.invariants_hold()); }

TEST(context_blob_publisher, a_second_publish_for_a_context_it_already_holds_is_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  ContextBlobPublisher publisher(fixture.mapped().platform());
  const GuestPageList blobs[] = {whole(), whole(), whole()};
  std::uint32_t ids[3] = {};
  CHECK_STATUS(publisher.publish(fixture.device(), 7u, blobs, 3u, ids), Status::ok);
  CHECK_STATUS(publisher.publish(fixture.device(), 7u, blobs, 3u, ids), Status::invalid_state); }

/* An allocation not resident, or not the whole blob, has an incomplete page
 * list; the publisher refuses before it creates anything. */
TEST(context_blob_publisher, an_incomplete_allocation_is_refused_and_nothing_is_created)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  ContextBlobPublisher publisher(fixture.mapped().platform());
  const GuestPageList blobs[] = {whole(), GuestPageList(), whole()};
  std::uint32_t ids[3] = {};
  CHECK_STATUS(publisher.publish(fixture.device(), 7u, blobs, 3u, ids), Status::invalid_parameter);
  CHECK(!publisher.holds(7u));
  CHECK_EQ(host.count_of(create_blob_type), 0u); }

/* A create the device refuses fails the publish and unrefs the blobs already
 * made, so the device is left holding none and the context is not held. */
TEST(context_blob_publisher, a_refused_create_unrefs_what_came_before)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  ContextBlobPublisher publisher(fixture.mapped().platform());
  const GuestPageList blobs[] = {whole(), whole(), whole()};
  std::uint32_t ids[3] = {};
  host.refuse_at(2);   /* the third create fails */
  CHECK_STATUS(publisher.publish(fixture.device(), 7u, blobs, 3u, ids), Status::not_supported);
  CHECK(!publisher.holds(7u));
  /* Three creates attempted, the two that took unref'd. */
  CHECK_EQ(host.count_of(create_blob_type), 3u);
  CHECK_EQ(host.count_of(unref_type), 2u); }

TEST(context_blob_publisher, teardown_unrefs_exactly_the_blobs_it_made)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  ContextBlobPublisher publisher(fixture.mapped().platform());
  const GuestPageList blobs[] = {whole(), whole(), whole()};
  std::uint32_t ids[3] = {};
  CHECK_STATUS(publisher.publish(fixture.device(), 7u, blobs, 3u, ids), Status::ok);
  CHECK_STATUS(publisher.retire(fixture.device(), 7u), Status::ok);
  CHECK(!publisher.holds(7u));
  CHECK_EQ(host.count_of(unref_type), 3u);
  /* Retiring a context it does not hold is refused. */
  CHECK_STATUS(publisher.retire(fixture.device(), 7u), Status::invalid_state); }

/* The argument guards: no blobs, no id buffer, a zero count, more than three. */
TEST(context_blob_publisher, bad_arguments_are_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  ContextBlobPublisher publisher(fixture.mapped().platform());
  const GuestPageList blobs[] = {whole(), whole(), whole()};
  std::uint32_t ids[3] = {};
  CHECK_STATUS(publisher.publish(fixture.device(), 7u, nullptr, 3u, ids), Status::invalid_parameter);
  CHECK_STATUS(publisher.publish(fixture.device(), 7u, blobs, 3u, nullptr), Status::invalid_parameter);
  CHECK_STATUS(publisher.publish(fixture.device(), 7u, blobs, 0u, ids), Status::invalid_parameter);
  CHECK_STATUS(publisher.publish(fixture.device(), 7u, blobs, 4u, ids), Status::invalid_parameter);
  CHECK_EQ(host.count_of(create_blob_type), 0u); }

/* Every slot taken, one context per slot; the next context has nowhere to go. */
TEST(context_blob_publisher, a_full_table_has_no_room_for_another_context)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  ContextBlobPublisher publisher(fixture.mapped().platform());
  const GuestPageList one[] = {whole()};
  std::uint32_t id = 0u;
  for (std::uint32_t c = 0u; c < ContextBlobPublisher::max_contexts; c++)
    CHECK_STATUS(publisher.publish(fixture.device(), c + 1u, one, 1u, &id), Status::ok);
  CHECK_STATUS(publisher.publish(fixture.device(), 0xFFFFu, one, 1u, &id), Status::no_space);
  CHECK(publisher.invariants_hold()); }

/* A complete list too large for the wide slot is refused by the encoder, and
 * nothing is created. */
TEST(context_blob_publisher, a_blob_wider_than_the_slot_is_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  ContextBlobPublisher publisher(fixture.mapped().platform());
  /* Non-adjacent pages, one run each, past what 0x9000 bytes of entries hold. */
  static std::uint64_t scattered[2400];
  for (std::size_t i = 0u; i < 2400u; i++) scattered[i] = (i + 1u) * 2u * 0x1000ull;
  const GuestPageList huge(scattered, 2400u);
  const GuestPageList blobs[] = {huge};
  std::uint32_t ids[1] = {};
  CHECK_STATUS(publisher.publish(fixture.device(), 7u, blobs, 1u, ids), Status::too_large);
  CHECK(!publisher.holds(7u));
  CHECK_EQ(host.count_of(create_blob_type), 0u); }

/* A device that answers nothing fails the publish; a device that answers
 * garbage does too. */
TEST(context_blob_publisher, a_device_that_will_not_answer_fails_the_publish)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  host.answer_nothing();
  ContextBlobPublisher publisher(fixture.mapped().platform());
  const GuestPageList blobs[] = {whole()};
  std::uint32_t ids[1] = {};
  CHECK(publisher.publish(fixture.device(), 7u, blobs, 1u, ids) != Status::ok);
  CHECK(!publisher.holds(7u)); }

TEST(context_blob_publisher, a_garbled_answer_fails_the_publish)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  host.answer_garbage();
  ContextBlobPublisher publisher(fixture.mapped().platform());
  const GuestPageList blobs[] = {whole()};
  std::uint32_t ids[1] = {};
  CHECK(publisher.publish(fixture.device(), 7u, blobs, 1u, ids) != Status::ok);
  CHECK(!publisher.holds(7u)); }

TEST(context_blob_publisher, the_invariant_catches_states_it_never_builds)
{ MappedVirtioFunction mapped;
  ContextBlobPublisher bad_count(mapped.platform());
  ContextBlobPublisherProbe::break_count(bad_count);
  CHECK(!bad_count.invariants_hold());
  ContextBlobPublisher bad_id(mapped.platform());
  ContextBlobPublisherProbe::break_next_id(bad_id);
  CHECK(!bad_id.invariants_hold());
  ContextBlobPublisher too_many(mapped.platform());
  ContextBlobPublisherProbe::break_count_high(too_many);
  CHECK(!too_many.invariants_hold()); }
