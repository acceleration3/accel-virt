#include "harness/host/mock_renderer_resources.hpp"

#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstring>
#include <unistd.h>

namespace av {

MockRendererResources::MockRendererResources(MockHost &qemu, const AvExecutorOps &executor)
    : qemu_(qemu), executor_(executor), descriptors_(0u), mappings_(0u), offered_(-1), lines_{},
      refuse_executor_(false), loads_(0u), path_{}
{
    for (int &line : lines_) line = -1;
}

MockRendererResources::~MockRendererResources()
{
    drop_interrupt_lines();
    close_offered();
    if (mappings_ != 0u)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "%u renderer region(s) never unmapped",
                                                mappings_);
    if (descriptors_ != 0u)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "%u renderer descriptor(s) never closed",
                                                descriptors_);
}

Status MockRendererResources::map_region(int descriptor, std::uint64_t size, void *&base)
{
    base = qemu_.guest_region_mapping(descriptor, size);
    if (base == nullptr) return Status::no_memory;
    mappings_++;
    return Status::ok;
}

void MockRendererResources::unmap_region(void *, std::uint64_t)
{
    if (mappings_ == 0u) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "a region unmapped that was never mapped");
        return;
    }
    mappings_--;
}

void MockRendererResources::close_descriptor(int descriptor)
{
    if (descriptor < 0 || ::close(descriptor) != 0 || descriptors_ == 0u) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "descriptor %d closed that was not open",
                                                descriptor);
        return;
    }
    descriptors_--;
}

void MockRendererResources::offer_host_memory(int descriptor)
{
    close_offered();
    offered_ = descriptor;
}

void MockRendererResources::close_offered()
{
    if (offered_ >= 0) close_descriptor(offered_);
    offered_ = -1;
}

void MockRendererResources::take_interrupt_lines(const int *descriptors, std::uint32_t count)
{
    drop_interrupt_lines();
    for (std::uint32_t line = 0u; line < count; line++) {
        if (line < max_lines) lines_[line] = descriptors[line];
        else if (descriptors[line] >= 0) close_descriptor(descriptors[line]);
    }
}

void MockRendererResources::drop_interrupt_lines()
{
    for (int &line : lines_) {
        if (line >= 0) close_descriptor(line);
        line = -1;
    }
}

Status MockRendererResources::load_executor(const char *path, std::uint32_t length, const AvExecutorOps *&ops)
{
    ops = nullptr;
    loads_++;
    const std::size_t kept = length < max_path - 1u ? length : max_path - 1u;
    std::memcpy(path_, path, kept);
    path_[kept] = '\0';
    if (refuse_executor_) {
        refuse_executor_ = false;
        return Status::not_found;
    }
    ops = &executor_;
    return Status::ok;
}

void MockRendererResources::adopt(std::uint32_t count)
{
    descriptors_ += count;
}

void MockRendererResources::refuse_executor()
{
    refuse_executor_ = true;
}

std::uint32_t MockRendererResources::live_descriptors() const
{
    return descriptors_;
}

std::uint32_t MockRendererResources::live_mappings() const
{
    return mappings_;
}

std::uint32_t MockRendererResources::interrupt_lines() const
{
    std::uint32_t held = 0u;
    for (const int line : lines_) held += line >= 0 ? 1u : 0u;
    return held;
}

std::uint32_t MockRendererResources::executor_loads() const
{
    return loads_;
}

const char *MockRendererResources::executor_path() const
{
    return path_;
}

}  // namespace av
