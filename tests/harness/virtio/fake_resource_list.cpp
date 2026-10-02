#include "harness/virtio/fake_resource_list.hpp"

#include "harness/framework/test_registry.hpp"

namespace av {

namespace {

constexpr std::size_t list_header = 4u;
constexpr std::size_t full_header = 16u;
constexpr std::size_t partial_count_at = 12u;
constexpr std::size_t partial_bytes = 20u;

enum : std::uint8_t { type_port = 1u, type_interrupt = 2u, type_memory = 3u, type_memory_large = 7u };
constexpr std::uint16_t interrupt_message = 0x02u;

}  // namespace

FakeResourceList::FakeResourceList()
    : bytes_(), size_(list_header), full_count_(0u), current_full_(0u), partial_count_(0u)
{
    begin_full_descriptor();
}

void FakeResourceList::put(std::size_t offset, std::uint64_t value, std::size_t width)
{
    if (offset + width > capacity) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "FakeResourceList overflow");
        return;
    }
    for (std::size_t i = 0u; i < width; i++) bytes_[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
}

void FakeResourceList::begin_full_descriptor()
{
    current_full_ = size_;
    partial_count_ = 0u;
    put(current_full_ + 8u, 1u, 2u);  /* PartialResourceList.Version */
    put(current_full_ + 10u, 1u, 2u); /* Revision */
    put(current_full_ + partial_count_at, 0u, 4u);
    size_ += full_header;
    put(0u, ++full_count_, 4u);
}

void FakeResourceList::add_partial(std::uint8_t type, std::uint16_t flags, std::uint64_t first,
                                   std::uint32_t second)
{
    put(size_, type, 1u);
    put(size_ + 2u, flags, 2u);
    put(size_ + 4u, first, 8u);
    put(size_ + 12u, second, 4u);
    size_ += partial_bytes;
    put(current_full_ + partial_count_at, ++partial_count_, 4u);
}

void FakeResourceList::add_memory(std::uint64_t start, std::uint32_t length)
{
    add_partial(type_memory, 0u, start, length);
}

void FakeResourceList::add_large_memory(std::uint64_t start, std::uint16_t flags, std::uint32_t stored)
{
    add_partial(type_memory_large, flags, start, stored);
}

/* Translated message interrupt: Level, Vector, Affinity - opaque to the driver. */
void FakeResourceList::add_message_interrupt() { add_partial(type_interrupt, interrupt_message, 0x60u, 0u); }

void FakeResourceList::add_line_interrupt() { add_partial(type_interrupt, 0u, 0x0Bu, 0u); }

void FakeResourceList::add_port(std::uint64_t start, std::uint32_t length)
{
    add_partial(type_port, 0u, start, length);
}

void FakeResourceList::set_list_count(std::uint32_t count) { put(0u, count, 4u); }

void FakeResourceList::set_partial_count(std::uint32_t count) { put(current_full_ + partial_count_at, count, 4u); }

const std::uint8_t *FakeResourceList::bytes() const { return bytes_; }

std::uint8_t *FakeResourceList::mutable_bytes() { return bytes_; }

std::size_t FakeResourceList::size() const { return size_; }

}  // namespace av
