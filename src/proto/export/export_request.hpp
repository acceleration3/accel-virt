/* A consumer's request on a device's export socket (docs/external-export.md):
 * one 32-byte datagram naming an operation, the guest's shared id of a
 * surface, and a tag of the consumer's own that the answer carries back - or,
 * for timing, the consumer's display's refresh and a moment it blanked at in
 * the surface's place. Everything in it came from another process. */
#ifndef AV_EXPORT_REQUEST_HPP
#define AV_EXPORT_REQUEST_HPP

#include "core/support/status.hpp"
#include "proto/export/export_operation.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExportRequest {
public:
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic = 0x51585641u;   /* "AVXQ" */
    static constexpr uint16_t major = 1u;
    static constexpr uint16_t minor = 2u;
    /* The heads a head request may name, the display clock's. */
    static constexpr uint64_t most_heads = 16u;

    ExportRequest();
    ExportRequest(ExportOperation operation, uint64_t surface, uint64_t tag);
    /* A timing request: the display refreshes at `refresh_millihz` and
     * blanked at `blank`, on the host's monotonic clock in nanoseconds. */
    ExportRequest(uint32_t refresh_millihz, uint64_t blank, uint64_t tag);

    ExportOperation operation() const;
    /* The guest's shared id; 0 for hello and timing, which name none. A head
     * request names head N as N + 1. */
    uint64_t        surface() const;
    /* A head request's head. */
    uint32_t        head() const;
    uint64_t        tag() const;
    /* A timing request's; 0 in any other. */
    uint32_t        refresh_millihz() const;
    uint64_t        blank() const;

    /* too_large for less room than encoded_size. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length, another magic, a non-zero reserved
     * word, an operation a consumer does not send, a hello naming a surface
     * or anything else naming none, a timing whose refresh the display clock
     * does not run at or that names no moment; not_supported for another major version,
     * whatever follows it, since its layout is not this one. Any minor is
     * this layout. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    bool fields_valid() const;

    ExportOperation operation_;
    uint64_t        surface_;
    uint64_t        tag_;
    uint32_t        refresh_millihz_;
    uint64_t        blank_;
};

}  // namespace av

#endif  // AV_EXPORT_REQUEST_HPP
