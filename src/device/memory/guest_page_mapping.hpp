#ifndef AV_GUEST_PAGE_MAPPING_HPP
#define AV_GUEST_PAGE_MAPPING_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobWindow;
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

    [[nodiscard]] Status map(ControlRequest &request, size_t offset, uint32_t entries);
    void                 release();
    uint64_t             length() const;
    uint32_t             extents() const;
    BlobWindow           window() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status map_entry(uint64_t address, uint64_t length);
    [[nodiscard]] Status add_extent(void *pointer, uint64_t length);
    [[nodiscard]] Status reserve(uint32_t capacity);

    Host     &host_;
    void    **pointers_;
    uint64_t *lengths_;
    uint32_t  capacity_;
    uint32_t count_;
    uint64_t length_;

    friend struct GuestPageMappingProbe;
};

}

#endif
