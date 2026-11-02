#ifndef AV_PRESENT_COPY_REQUEST_HPP
#define AV_PRESENT_COPY_REQUEST_HPP

#include "core/support/status.hpp"
#include "proto/dma/present_operation.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class PresentCopyRequest {
public:
    static constexpr uint32_t type = 0x02F2u;
    static constexpr size_t   encoded_size = 56u;
    static constexpr size_t   entry_size = 16u;
    static constexpr uint32_t max_entries = 16384u;
    static constexpr uint32_t max_regions = 256u;
    static constexpr uint32_t flag_swap_red_blue = 1u;

    PresentCopyRequest();
    PresentCopyRequest(PresentOperation operation, uint32_t flags, uint32_t colour, uint32_t regions,
                       uint32_t source_entries, uint32_t source_pitch, uint32_t destination_entries,
                       uint32_t destination_pitch);

    void                 write(WireWriter &writer) const;
    [[nodiscard]] Status decode(WireReader &reader);

    PresentOperation operation() const;
    bool             swaps_red_and_blue() const;
    uint32_t         colour() const;
    uint32_t         regions() const;
    uint32_t         source_entries() const;
    uint32_t         source_pitch() const;
    uint32_t         destination_entries() const;
    uint32_t         destination_pitch() const;
    size_t           destination_entries_offset() const;
    size_t           regions_offset() const;
    size_t           size() const;

private:
    static constexpr size_t   header_size = 24u;
    static constexpr uint32_t pixel_bytes = 4u;

    bool holds() const;

    PresentOperation operation_;
    uint32_t         flags_;
    uint32_t         colour_;
    uint32_t         regions_;
    uint32_t         source_entries_;
    uint32_t         source_pitch_;
    uint32_t         destination_entries_;
    uint32_t         destination_pitch_;
};

}

#endif
