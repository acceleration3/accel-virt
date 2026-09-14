#include "harness/fullstack/virtio_bridge.hpp"

#include "harness/framework/test_registry.hpp"
#include "harness/loopback/loopback_device.hpp"

namespace av {

namespace {

constexpr std::uint32_t header_bytes = 24u;

}  // namespace

VirtioBridge::VirtioBridge(MockPlatform &platform, FakeCommonConfig &common, LoopbackDevice &device)
    : platform_(platform), common_(common), device_(device), base_(), views_(), placed_{}, completed_{},
      by_core_(0u), request_{}
{
    common_.on_status([](void *context, std::uint8_t status) { static_cast<VirtioBridge *>(context)->follow_status(status); },
                      this);
}

VirtioBridge::~VirtioBridge() { common_.on_status(nullptr, nullptr); }

/* virtio_pci_common_write: set_status sees every value, and 0 then resets. */
void VirtioBridge::follow_status(std::uint8_t status)
{
    device_.device().status(status);
    if (status == 0u) device_.device().reset();
}

std::uint64_t VirtioBridge::length() const { return region_bytes; }

std::uint8_t VirtioBridge::read_u8(std::uint64_t offset)
{
    unexpected("read_u8", offset);
    return 0u;
}

std::uint16_t VirtioBridge::read_u16(std::uint64_t offset)
{
    unexpected("read_u16", offset);
    return 0u;
}

std::uint32_t VirtioBridge::read_u32(std::uint64_t offset)
{
    unexpected("read_u32", offset);
    return 0u;
}

void VirtioBridge::write_u8(std::uint64_t offset, std::uint8_t) { unexpected("write_u8", offset); }
void VirtioBridge::write_u32(std::uint64_t offset, std::uint32_t) { unexpected("write_u32", offset); }
void VirtioBridge::write_u64(std::uint64_t offset, std::uint64_t) { unexpected("write_u64", offset); }

/* virtio 1.2 4.1.4.4: without VIRTIO_F_NOTIFICATION_DATA the driver stores
 * the queue's index. */
void VirtioBridge::write_u16(std::uint64_t, std::uint16_t value)
{
    if (value >= served_queues) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "the driver notified queue %u, which it only reads",
                                                value);
        return;
    }
    serve(value);
}

void VirtioBridge::unexpected(const char *access, std::uint64_t offset)
{
    TestRegistry::instance().record_failure(__FILE__, __LINE__, "unexpected %s at notify offset 0x%llx", access,
                                            static_cast<unsigned long long>(offset));
}

FakeVirtqueueDevice &VirtioBridge::view(std::uint16_t queue)
{
    const FakeCommonConfig::Queue &now = common_.queue(queue);
    FakeCommonConfig::Queue       &placed = placed_[queue];
    const bool moved = now.desc != placed.desc || now.driver != placed.driver || now.device != placed.device ||
                       now.size != placed.size;
    if (!views_[queue].has_value() || moved) {
        views_[queue].reset();
        views_[queue].emplace(platform_, now.desc, now.driver, now.device, now.size);
        placed = now;
    }
    return *views_[queue];
}

void VirtioBridge::serve(std::uint16_t queue)
{
    FakeVirtqueueDevice &device = view(queue);
    const std::uint16_t  completed =
        queue == cursor_queue
            ? device.complete_all(0u)
            : device.answer_all(request_, request_capacity,
                                [](void *context, const std::uint8_t *request, std::uint32_t length,
                                   std::uint8_t *answer, std::uint32_t room) {
                                    return static_cast<VirtioBridge *>(context)->answer(request, length, answer,
                                                                                        room);
                                },
                                this);
    if (completed != 0u) completed_[queue] = true;
}

std::uint32_t VirtioBridge::answer(const std::uint8_t *request, std::uint32_t length, std::uint8_t *answer,
                                   std::uint32_t room)
{
    if (length >= header_bytes && room != 0u) {
        const std::size_t answered = device_.exchange(request, length, answer, room);
        if (answered != 0u) {
            by_core_++;
            return static_cast<std::uint32_t>(answered);
        }
    }
    return base_.answer(request, length, answer, room);
}

bool VirtioBridge::take_completion(std::uint16_t queue)
{
    const bool completed = queue < served_queues && completed_[queue];
    if (completed) completed_[queue] = false;
    return completed;
}

std::uint32_t VirtioBridge::answered_by_core() const { return by_core_; }
std::uint32_t VirtioBridge::answered_by_base() const { return base_.answered(); }

}  // namespace av
