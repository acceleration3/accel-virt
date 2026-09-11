#include "fake_virtqueue_device.hpp"

#include "test_registry.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

namespace {

constexpr std::uint16_t flag_next = 1u;

}  // namespace

FakeVirtqueueDevice::FakeVirtqueueDevice(MockPlatform &platform, std::uint64_t descriptors,
                                         std::uint64_t driver_area, std::uint64_t device_area,
                                         std::uint16_t size)
    : platform_(platform), descriptors_(descriptors), driver_area_(driver_area),
      device_area_(device_area), size_(size), last_available_(0u), used_index_(0u), seen_{},
      seen_count_(0u), requests_{}, request_count_(0u) {}

std::uint8_t *FakeVirtqueueDevice::at(std::uint64_t address, std::size_t bytes) const
{
    auto *memory = static_cast<std::uint8_t *>(platform_.translate_physical(address, bytes));
    if (memory == nullptr)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "device access to unmapped 0x%llx",
                                                static_cast<unsigned long long>(address));
    return memory;
}

std::uint64_t FakeVirtqueueDevice::read(std::uint64_t address, unsigned width) const
{
    const std::uint8_t *memory = at(address, width);
    std::uint64_t value = 0u;
    for (unsigned i = 0u; memory != nullptr && i < width; i++) value |= std::uint64_t{memory[i]} << (8u * i);
    return value;
}

void FakeVirtqueueDevice::write(std::uint64_t address, std::uint64_t value, unsigned width)
{
    std::uint8_t *memory = at(address, width);
    for (unsigned i = 0u; memory != nullptr && i < width; i++)
        memory[i] = static_cast<std::uint8_t>(value >> (8u * i));
}

std::uint16_t FakeVirtqueueDevice::available_index() const
{
    return static_cast<std::uint16_t>(read(driver_area_ + 2u, 2u));
}

std::uint16_t FakeVirtqueueDevice::head(std::uint16_t available_position) const
{
    return static_cast<std::uint16_t>(read(driver_area_ + 4u + 2u * (available_position % size_), 2u));
}

void FakeVirtqueueDevice::walk(std::uint16_t chain, bool keep_request, std::uint64_t &writable,
                               std::uint32_t &writable_length)
{
    constexpr std::uint16_t flag_write = 2u;
    std::uint16_t descriptor = chain;
    bool          request_kept = !keep_request;
    writable = 0u;
    writable_length = 0u;
    for (std::uint16_t hops = 0u; hops <= size_; hops++) {
        const std::uint64_t base = descriptors_ + 16u * descriptor;
        const std::uint16_t flags = static_cast<std::uint16_t>(read(base + 12u, 2u));
        const std::uint64_t address = read(base, 8u);
        const std::uint32_t length = static_cast<std::uint32_t>(read(base + 8u, 4u));
        if (seen_count_ < max_seen) seen_[seen_count_++] = Seen{address, length, flags};
        if ((flags & flag_write) != 0u) {
            writable = address;
            writable_length = length;
        } else if (!request_kept && request_count_ < max_requests) {
            const std::uint32_t kept = length < request_bytes ? length : request_bytes;
            const std::uint8_t *bytes = at(address, kept);
            for (std::uint32_t byte = 0u; bytes != nullptr && byte < kept; byte++)
                requests_[request_count_][byte] = bytes[byte];
            request_count_++;
            request_kept = true;
        }
        if ((flags & flag_next) == 0u) break;
        descriptor = static_cast<std::uint16_t>(read(base + 14u, 2u));
        if (descriptor >= size_)
            TestRegistry::instance().record_failure(__FILE__, __LINE__, "driver chain leaves the table");
    }
}

std::uint16_t FakeVirtqueueDevice::complete_all(std::uint32_t written)
{
    std::uint16_t completed = 0u;
    const std::uint16_t available = available_index();
    while (last_available_ != available) {
        const std::uint16_t chain = head(last_available_);
        std::uint64_t writable = 0u;
        std::uint32_t writable_length = 0u;
        walk(chain, false, writable, writable_length);
        write_used(used_index_, chain, written);
        set_used_index(static_cast<std::uint16_t>(used_index_ + 1u));
        last_available_++;
        completed++;
    }
    return completed;
}

std::uint16_t FakeVirtqueueDevice::respond_all(std::uint32_t type)
{
    constexpr std::uint32_t header = 24u;
    std::uint16_t completed = 0u;
    const std::uint16_t available = available_index();
    while (last_available_ != available) {
        const std::uint16_t chain = head(last_available_);
        std::uint64_t writable = 0u;
        std::uint32_t writable_length = 0u;
        walk(chain, true, writable, writable_length);
        const std::uint32_t written = writable_length >= header ? header : 0u;
        if (written != 0u) {
            write(writable, type, 4u);
            for (std::uint32_t byte = 4u; byte < header; byte++) write(writable + byte, 0u, 1u);
        }
        write_used(used_index_, chain, written);
        set_used_index(static_cast<std::uint16_t>(used_index_ + 1u));
        last_available_++;
        completed++;
    }
    return completed;
}

std::uint32_t       FakeVirtqueueDevice::requests() const { return request_count_; }
const std::uint8_t *FakeVirtqueueDevice::request(std::uint32_t index) const
{
    return requests_[index < max_requests ? index : 0u];
}
std::uint32_t FakeVirtqueueDevice::request_type(std::uint32_t index) const
{
    const std::uint8_t *bytes = request(index);
    return std::uint32_t{bytes[0]} | std::uint32_t{bytes[1]} << 8u | std::uint32_t{bytes[2]} << 16u |
           std::uint32_t{bytes[3]} << 24u;
}

void FakeVirtqueueDevice::write_used(std::uint16_t position, std::uint32_t id, std::uint32_t length)
{
    const std::uint64_t element = device_area_ + 4u + 8u * (position % size_);
    write(element, id, 4u);
    write(element + 4u, length, 4u);
}

void FakeVirtqueueDevice::set_used_index(std::uint16_t index)
{
    used_index_ = index;
    write(device_area_ + 2u, index, 2u);
}

const FakeVirtqueueDevice::Seen &FakeVirtqueueDevice::seen(std::uint32_t index) const { return seen_[index]; }
std::uint32_t FakeVirtqueueDevice::seen_count() const { return seen_count_; }

}  // namespace av
