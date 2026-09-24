/* The escape record a process sends to publish its own ring's tail: from then
 * on it stores the tail itself before each submission, so the device may take
 * a batch before the scheduler has handed the driver its packet, and the
 * driver adopts the tail it finds rather than writing its own (the doorbell
 * path's shared tail, reached through D3DKMTSubmitCommand). */
#ifndef AV_TAIL_PUBLISH_RECORD_HPP
#define AV_TAIL_PUBLISH_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class TailPublishRecord {
public:
    /* dxgkrnl answers STATUS_NOT_SUPPORTED to an 8-byte private escape without
     * handing it to the driver; one the size of HostVisibleBindRecord gets
     * through. */
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x50564B52u;  /* "RVKP" */

    TailPublishRecord();

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
};

}  // namespace av

#endif  // AV_TAIL_PUBLISH_RECORD_HPP
