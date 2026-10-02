/* A shared object's release, told to the device over a real control queue: the
 * driver's own request type with the id where the device reads it, and the
 * device's answer - a refusal, bytes that are no answer, or none at all - is
 * what send reports. */
#include "miniport/memory/shared_object_release.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/shared_release_request.hpp"

#include <cstdint>
#include <cstring>
#include <optional>

using av::FakeVirtqueueDevice;
using av::GpuResponse;
using av::Irql;
using av::MappedVirtioFunction;
using av::SharedObjectRelease;
using av::SharedReleaseRequest;
using av::Status;
using av::VirtioGpuDevice;
using av::Violation;

namespace {

constexpr std::uint16_t control_queue = 0u;

class Host {
public:
    explicit Host(MappedVirtioFunction &mapped) : queue_(mapped.queue_device(control_queue))
    { mapped.platform().on_stall(&Host::stall, this); }

    void refuse_next() { refuse_ = true; }
    void answer_nothing() { silent_ = true; }
    void answer_garbage() { garbage_ = true; }
    std::uint32_t answered() const { return answered_; }
    FakeVirtqueueDevice &queue() { return queue_; }

private:
    static void stall(void *context)
    {
        auto *const host = static_cast<Host *>(context);
        if (host->silent_ || host->queue_.pending_type() == 0u) return;
        host->answered_++;
        if (host->garbage_) {
            const std::uint8_t rubbish[8] = {0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE};
            static_cast<void>(host->queue_.respond_all(rubbish, sizeof rubbish));
            return;
        }
        GpuResponse answer;
        answer.set(host->refuse_ ? GpuResponse::err_invalid_parameter : GpuResponse::ok_nodata);
        host->refuse_ = false;
        static_cast<void>(host->queue_.respond_all(answer.bytes(), static_cast<std::uint32_t>(answer.length())));
    }

    FakeVirtqueueDevice queue_;
    std::uint32_t       answered_ = 0u;
    bool                refuse_ = false;
    bool                garbage_ = false;
    bool                silent_ = false;
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

}  // namespace

TEST(shared_object_release, sends_the_driver_s_request_naming_the_id_and_takes_the_device_s_ok)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  SharedObjectRelease release(fixture.mapped().platform());
  CHECK_STATUS(release.send(fixture.device(), 0x7766554433221100u), Status::ok);
  CHECK_EQ(host.answered(), 1u);
  const std::uint32_t last = host.queue().requests() - 1u;
  CHECK_EQ(host.queue().request_type(last), SharedReleaseRequest::type);
  std::uint64_t id = 0u;
  std::memcpy(&id, host.queue().request(last) + 24u, sizeof id);
  CHECK_EQ(id, 0x7766554433221100u); }

TEST(shared_object_release, a_refusal_is_not_supported_and_a_non_answer_is_the_decoder_s)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  SharedObjectRelease release(fixture.mapped().platform());
  host.refuse_next();
  CHECK_STATUS(release.send(fixture.device(), 5u), Status::not_supported);
  host.answer_garbage();
  CHECK(release.send(fixture.device(), 5u) != Status::ok);
  CHECK_EQ(host.answered(), 2u); }

TEST(shared_object_release, a_device_that_never_answers_is_the_exchange_s_answer)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  host.answer_nothing();
  SharedObjectRelease release(fixture.mapped().platform());
  CHECK(release.send(fixture.device(), 5u) != Status::ok);
  CHECK_EQ(host.answered(), 0u); }

TEST(shared_object_release, is_passive_only)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  SharedObjectRelease release(fixture.mapped().platform());
  fixture.mapped().platform().set_irql(Irql::dispatch);
  static_cast<void>(release.send(fixture.device(), 5u));
  fixture.mapped().platform().set_irql(Irql::passive);
  CHECK(fixture.mapped().platform().claim(Violation::irql_ceiling));
  /* The device's own ceilings, reached under it, and the wait for its turn
   * at the queue, which blocks. */
  while (fixture.mapped().platform().claim(Violation::irql_ceiling)) {}
  CHECK(fixture.mapped().platform().claim(Violation::blocking_at_dispatch)); }
