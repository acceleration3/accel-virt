#ifndef AV_RESOURCE_LIST_HPP
#define AV_RESOURCE_LIST_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct ResourceListProbe;

class ResourceList {
public:
    static constexpr uint32_t max_full_descriptors    = 4u;
    static constexpr uint32_t max_partial_descriptors = 64u;
    static constexpr uint32_t max_memory_ranges       = 16u;

    static constexpr size_t list_header_bytes        = 4u;
    static constexpr size_t full_header_bytes        = 16u;
    static constexpr size_t partial_count_offset     = 12u;
    static constexpr size_t partial_descriptor_bytes = 20u;
    static constexpr size_t partial_flags_offset     = 2u;
    static constexpr size_t memory_start_offset      = 4u;
    static constexpr size_t memory_length_offset     = 12u;

    static constexpr size_t max_bytes =
        list_header_bytes +
        max_full_descriptors * (full_header_bytes + max_partial_descriptors * partial_descriptor_bytes);

    explicit ResourceList(Platform &platform);
    ResourceList(const ResourceList &) = delete;
    ResourceList &operator=(const ResourceList &) = delete;

    [[nodiscard]] Status initialize(const uint8_t *bytes, size_t length);

    [[nodiscard]] Status find_memory(uint64_t start, uint64_t minimum_length, uint64_t &length) const;
    uint32_t             message_interrupts() const;

    bool invariants_hold() const;

private:
    class Cursor;

    Status read_full_descriptor(Cursor &cursor, uint32_t &partial_total);
    Status read_partial_descriptor(Cursor &cursor);
    Status add_memory(uint64_t start, uint64_t length);

    Platform   &platform_;
    uint64_t    memory_starts_[max_memory_ranges];
    uint64_t    memory_lengths_[max_memory_ranges];
    uint32_t    memory_count_;
    uint32_t    message_interrupts_;

    friend struct ResourceListProbe;
};

}

#endif
