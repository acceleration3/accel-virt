/* A guest-backed blob's pages, mapped for the device once, on the main loop, and
 * held until released, so workers can read and write them without the memory
 * API. The guest names them as (address, length) entries; each entry becomes as
 * many mappings as QEMU needs to cross region boundaries. Only guest RAM is
 * accepted: a bounce buffer is a copy, and a ring must be shared memory. */
#ifndef AV_GUEST_PAGE_MAPPING_HPP
#define AV_GUEST_PAGE_MAPPING_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ControlRequest;
class Host;
struct GuestPageMappingProbe;

class GuestPageMapping {
public:
    static constexpr uint32_t max_entries = 16384u;
    static constexpr size_t   entry_size = 16u;

    explicit GuestPageMapping(Host &host);
    ~GuestPageMapping();
    GuestPageMapping(const GuestPageMapping &) = delete;
    GuestPageMapping &operator=(const GuestPageMapping &) = delete;

    /* Main loop. The `entries` entries at `offset` of `request`. invalid_parameter
     * for no or too many entries, a short list, an empty or wrapping entry;
     * not_found for memory that is not mapped; not_supported for memory that is
     * not guest RAM; no_memory when the host cannot hold the mappings; invalid_state
     * when already mapped. Holds nothing on failure. */
    [[nodiscard]] Status map(ControlRequest &request, size_t offset, uint32_t entries);
    void                 release();
    uint64_t             length() const;
    uint32_t             extents() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status map_entry(uint64_t address, uint64_t length);
    [[nodiscard]] Status add_extent(void *pointer, uint64_t length);
    /* Room for `capacity` extents: no_memory, changing nothing, when the host has none. */
    [[nodiscard]] Status reserve(uint32_t capacity);

    Host     &host_;
    /* Each extent's mapped pointer and length, in one host block. */
    void    **pointers_;
    uint64_t *lengths_;
    uint32_t  capacity_;
    uint32_t count_;
    uint64_t length_;

    friend struct GuestPageMappingProbe;
};

}  // namespace av

#endif  // AV_GUEST_PAGE_MAPPING_HPP
