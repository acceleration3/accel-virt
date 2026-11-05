#include "core/platform/platform.hpp"
#include "miniport/adapter/capset_exchange.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstdint>
#include <optional>

using av::CapsetExchange;
using av::CapsetInfo;
using av::FakeVirtqueueDevice;
using av::GpuIdentity;
using av::GpuResponse;
using av::MappedVirtioFunction;
using av::ProtocolVersion;
using av::Status;
using av::SubApi;
using av::VirtioGpuDevice;

namespace {

constexpr std::uint8_t uuid[GpuIdentity::uuid_bytes] = {7};
constexpr std::uint16_t control_queue = 0u;

CapsetInfo host_capset()
{
    return CapsetInfo(ProtocolVersion(ProtocolVersion::current_major, ProtocolVersion::current_minor),
                      static_cast<std::uint32_t>(SubApi::vulkan), 64u, 12u, 24u, 1u,
                      GpuIdentity(0x10DEu, 0x2204u, GpuIdentity::discrete_gpu, uuid, 1u << 20, 0u, "the host's"));
}

class Host {
public:
    Host(MappedVirtioFunction &mapped, std::uint32_t refuse_at = 0u)
        : queue_(mapped.queue_device(control_queue)), refuse_at_(refuse_at)
    {
        mapped.platform().on_stall(&Host::stall, this);
    }

    std::uint32_t answered() const { return answered_; }

private:
    static void stall(void *context)
    {
        auto *const host = static_cast<Host *>(context);
        GpuResponse answer;
        host->answered_++;
        if (host->answered_ == host->refuse_at_) {
            answer.set(GpuResponse::err_unspec);
        } else if (host->answered_ == 1u) {
            answer.set_capset_info(CapsetInfo::capset_id, 2u, CapsetInfo::encoded_size);
        } else {
            answer.set_capset(host_capset());
        }
        static_cast<void>(host->queue_.respond_all(answer.bytes(), static_cast<std::uint32_t>(answer.length())));
    }

    FakeVirtqueueDevice queue_;
    std::uint32_t       refuse_at_;
    std::uint32_t       answered_ = 0u;
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

}

TEST(capset_exchange_read, asks_the_device_and_answers_what_it_said)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  CapsetExchange exchange(fixture.mapped().platform(), fixture.device());
  CapsetInfo capset;
  CHECK_STATUS(exchange.read(capset), Status::ok);
  CHECK_EQ(capset.gpu().vendor_id(), 0x10DEu);
  CHECK_EQ(capset.gpu().uuid()[0], 7u);
  CHECK_EQ(capset.max_contexts(), 64u);
  CHECK_EQ(host.answered(), 2u); }

TEST(capset_exchange_read, a_refused_capset_is_not_supported)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped(), 2u);
  CapsetExchange exchange(fixture.mapped().platform(), fixture.device());
  CapsetInfo capset;
  CHECK_STATUS(exchange.read(capset), Status::not_supported);
  CHECK_EQ(capset.max_contexts(), 0u); }

TEST(capset_exchange_read, a_device_that_answers_nothing_is_not_found)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CapsetExchange exchange(fixture.mapped().platform(), fixture.device());
  CapsetInfo capset;
  CHECK_STATUS(exchange.read(capset), Status::not_found); }

TEST(capset_exchange_read, a_failed_exchange_stays_failed)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  CapsetExchange exchange(fixture.mapped().platform(), fixture.device());
  CapsetInfo capset;
  CHECK_STATUS(exchange.read(capset), Status::not_found);
  Host host(fixture.mapped());
  CHECK_STATUS(exchange.read(capset), Status::invalid_state);
  CHECK_EQ(host.answered(), 0u); }
