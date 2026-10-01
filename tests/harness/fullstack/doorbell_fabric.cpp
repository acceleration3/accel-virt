#include "harness/fullstack/doorbell_fabric.hpp"

#include "harness/platform/mock_platform.hpp"

#include <cstring>

namespace av {

namespace {

/* What a store to a doorbell is on this device: one four-byte write at offset
 * 0 of the page (av_device.h). */
std::uint32_t &word_of(std::uint8_t *page)
{
    return *reinterpret_cast<std::uint32_t *>(page);
}

}  // namespace

DoorbellFabric::DoorbellFabric(MockPlatform &platform)
    : platform_(platform), bytes_{}, taken_{}, kernel_{}, delivered_(0u), dropped_(0u)
{
}

std::uint32_t DoorbellFabric::index_of(const void *page) const
{
    for (std::uint32_t index = 0u; index < max_pages; index++)
        if (taken_[index] && bytes_[index] == page) return index;
    return max_pages;
}

void *DoorbellFabric::reserve()
{
    for (std::uint32_t index = 0u; index < max_pages; index++) {
        if (taken_[index]) continue;
        std::memset(bytes_[index], 0, page_bytes);
        taken_[index] = true;
        kernel_[index] = nullptr;
        return bytes_[index];
    }
    return nullptr;
}

void DoorbellFabric::aim(void *page, void *kernel)
{
    const std::uint32_t index = index_of(page);
    if (index != max_pages) kernel_[index] = kernel;
}

void DoorbellFabric::blind(void *page)
{
    const std::uint32_t index = index_of(page);
    if (index != max_pages) kernel_[index] = nullptr;
}

void DoorbellFabric::release(void *page)
{
    const std::uint32_t index = index_of(page);
    if (index == max_pages) return;
    /* A store still in the page when it is given back is one the device never
     * saw, and is counted as one that reached nothing. */
    if (word_of(bytes_[index]) != 0u) dropped_++;
    taken_[index] = false;
    kernel_[index] = nullptr;
}

std::uint32_t DoorbellFabric::deliver()
{
    std::uint32_t carried = 0u;
    for (std::uint32_t index = 0u; index < max_pages; index++) {
        if (!taken_[index]) continue;
        std::uint32_t &word = word_of(bytes_[index]);
        if (word == 0u) continue;
        const std::uint32_t value = word;
        word = 0u;
        if (kernel_[index] == nullptr) {
            dropped_++;
            continue;
        }
        platform_.write_register_u32(kernel_[index], value);
        carried++;
    }
    delivered_ += carried;
    return carried;
}

std::uint32_t DoorbellFabric::delivered() const
{
    return delivered_;
}

std::uint32_t DoorbellFabric::dropped() const
{
    return dropped_;
}

bool DoorbellFabric::invariants_hold() const
{
    for (std::uint32_t index = 0u; index < max_pages; index++) {
        if (!taken_[index] && kernel_[index] != nullptr) return false;
        for (std::uint32_t other = index + 1u; other < max_pages; other++)
            if (kernel_[index] != nullptr && kernel_[index] == kernel_[other]) return false;
    }
    return true;
}

}  // namespace av
