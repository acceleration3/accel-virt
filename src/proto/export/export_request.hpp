/* A consumer's request on a device's export socket (docs/external-export.md):
 * one 32-byte datagram naming an operation, the guest's shared id of a
 * surface, and a tag of the consumer's own that the answer carries back - or,
 * for timing, how far to shift the guest's blanks in the surface's place; an export or a sync may say in its flags that the
 * consumer imports on another GPU than the surface's. Everything in it came from another process. */
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
    static constexpr uint16_t minor = 5u;
    /* The heads a head request may name, the display clock's. */
    static constexpr uint64_t most_heads = 16u;
    /* An export's or a sync's flag: the consumer imports on another GPU, so
     * it is handed a copy of the surface in linear system memory. */
    static constexpr uint32_t other_gpu = 1u;
    /* The rates a display blanks at, in microhertz: the display clock's 1 Hz to
     * 1 kHz, which an answer's refresh stays within. */
    static constexpr uint32_t least_refresh_microhz = 1000000u;
    static constexpr uint32_t most_refresh_microhz = 1000000000u;
    /* A timing's shift is less than this either way. */
    static constexpr int64_t  most_shift_ns = 1000000000;

    ExportRequest();
    ExportRequest(ExportOperation operation, uint64_t surface, uint64_t tag);
    /* The same with `flags`, which only an export and a sync may set. */
    ExportRequest(ExportOperation operation, uint64_t surface, uint64_t tag, uint32_t flags);
    /* A timing request: from the next blank on, blank `shift_ns` earlier than
     * the schedule would have (negative: later), at the rate the guest runs. */
    ExportRequest(int64_t shift_ns, uint64_t tag);

    ExportOperation operation() const;
    /* The guest's shared id; 0 for hello and timing, which name none. A head
     * request names head N as N + 1. */
    uint64_t        surface() const;
    /* A head request's head. */
    uint32_t        head() const;
    /* An export's or a sync's; 0 in any other. */
    uint32_t        flags() const;
    bool            imports_on_other_gpu() const;
    uint64_t        tag() const;
    /* A timing request's; 0 in any other. */
    int64_t         shift_ns() const;

    /* too_large for less room than encoded_size. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length, another magic, a flag an operation
     * does not take, a reserved word that is not 0, an operation a consumer does not send, a hello naming a surface
     * or anything else naming none, a timing whose shift is a second or more either way; not_supported for another major version,
     * whatever follows it, since its layout is not this one. Any minor is
     * this layout. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    bool fields_valid() const;

    ExportOperation operation_;
    uint64_t        surface_;
    uint64_t        tag_;
    int64_t         shift_ns_;
    uint32_t        flags_;
};

}  // namespace av

#endif  // AV_EXPORT_REQUEST_HPP
