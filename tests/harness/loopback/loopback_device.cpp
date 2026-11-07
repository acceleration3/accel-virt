#include "harness/loopback/loopback_device.hpp"

#include "core/support/text_buffer.hpp"
#include "device/module/driver_status.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstring>
#include <sched.h>
#include <time.h>

namespace av {

namespace {

std::uint64_t now_ms()
{
    timespec time = {};
    clock_gettime(CLOCK_MONOTONIC, &time);
    return static_cast<std::uint64_t>(time.tv_sec) * 1000u + static_cast<std::uint64_t>(time.tv_nsec) / 1000000u;
}

}

LoopbackDevice::LoopbackDevice(MockPlatform &guest, const std::uint8_t *gpu, const char *record)
    : guest_(guest), host_(), executor_(AV_EXECUTOR_SUB_API_VULKAN), device_(host_), doorbells_(host_)
{
    realize(executor_.table(), gpu, record);
}

LoopbackDevice::LoopbackDevice(MockPlatform &guest, const AvExecutorOps &executor, const std::uint8_t *gpu)
    : guest_(guest), host_(), executor_(AV_EXECUTOR_SUB_API_VULKAN), device_(host_), doorbells_(host_)
{
    realize(executor, gpu, nullptr);
}

void LoopbackDevice::realize(const AvExecutorOps &executor, const std::uint8_t *gpu, const char *record)
{
    constexpr std::uint64_t nanoseconds_per_read = 1000u;
    host_.advance_clock_per_read(nanoseconds_per_read);
    host_.add_gpu(gpu);
    host_.set_hostmem_size(hostmem_bytes);
    host_.translate_guest_with([](void *context, std::uint64_t address, std::uint64_t length) {
        return static_cast<MockPlatform *>(context)->translate_physical(address, static_cast<std::size_t>(length));
    }, &guest_);
    char       bytes[128];
    TextBuffer message(bytes, sizeof bytes);
    const AvRecordingOptions recording = {record, 0u, "all", 0};
    static_cast<void>(
        device_.realize("auto", false, &executor, record != nullptr ? &recording : nullptr, nullptr,
                        nullptr, 0u, 0u, message));
    device_.status(DriverStatus::driver_ok);
}

LoopbackDevice::~LoopbackDevice()
{
    executor_.release();
    device_.reset();
}

MockHost          &LoopbackDevice::host() { return host_; }
MockExecutorOps   &LoopbackDevice::executor() { return executor_; }
VirtioGpuAv      &LoopbackDevice::device() { return device_; }
LoopbackDoorbells &LoopbackDevice::doorbells() { return doorbells_; }

std::size_t LoopbackDevice::exchange(const std::uint8_t *request, std::size_t length, std::uint8_t *answer,
                                     std::size_t capacity)
{
    MockRequest mock(request, length);
    std::uint32_t type = 0u;
    std::memcpy(&type, request, sizeof type);
    if (!device_.control(&mock, type)) return 0u;
    const std::size_t answered = mock.response_size() < capacity ? mock.response_size() : capacity;
    std::memcpy(answer, mock.response_bytes(), answered);
    return answered;
}

bool LoopbackDevice::raised_past(std::uint32_t line, std::uint32_t seen)
{
    const std::uint64_t deadline = now_ms() + patience_ms;
    while (host_.raised(line) <= seen && now_ms() < deadline) sched_yield();
    return host_.raised(line) > seen;
}

bool LoopbackDevice::holding(std::uint32_t count)
{
    const std::uint64_t deadline = now_ms() + patience_ms;
    while (executor_.waiting() < count && now_ms() < deadline) sched_yield();
    return executor_.waiting() >= count;
}

}
