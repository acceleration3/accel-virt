/* The device's answer on its export socket, and the one event it sends unasked
 * (docs/external-export.md): one 64-byte datagram - the operation answered,
 * a status, the surface and the consumer's tag, then a 32-byte body the
 * operation decides - with a dma-buf or a sync_file beside it when the answer
 * hands one over. The device writes it; a consumer, which reads it, holds it
 * to the same rules. */
#ifndef AV_EXPORT_ANSWER_HPP
#define AV_EXPORT_ANSWER_HPP

#include "core/support/status.hpp"
#include "proto/export/export_greeting.hpp"
#include "proto/export/export_operation.hpp"
#include "proto/export/exported_layout.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExportRequest;

class ExportAnswer {
public:
    static constexpr size_t   encoded_size = 64u;
    static constexpr uint32_t magic = 0x41585641u;   /* "AVXA" */
    static constexpr uint16_t major = 1u;
    static constexpr uint16_t minor = 1u;

    /* A request that did not decode, refused as malformed. */
    ExportAnswer();
    /* A request that did not decode, refused with `status`: malformed, or
     * not_supported for another major version. It names no surface and no
     * tag, since neither could be read. */
    explicit ExportAnswer(Status status);
    /* The answer to `request`: refused with any status but ok, or ok - which
     * for hello, export and sync then carries what greet, describe or count
     * gives it. */
    ExportAnswer(const ExportRequest &request, Status status);
    /* The event: `surface` gone for `reason` - every surface, as 0, at a
     * reset. */
    ExportAnswer(ExportRevocation reason, uint64_t surface);

    void greet(const ExportGreeting &greeting);
    void describe(const ExportedLayout &layout);
    void count(uint64_t writes);

    ExportOperation       operation() const;
    Status                status() const;
    uint64_t              surface() const;
    uint64_t              tag() const;
    const ExportGreeting &greeting() const;
    const ExportedLayout &layout() const;
    /* The copies into the surface the host has run, as a sync answers. */
    uint64_t              writes() const;
    /* A revocation's reason; 0 in any other answer. */
    uint32_t              reason() const;
    /* What travels beside it: the dma-buf of an export or the sync_file of a
     * sync answered ok, and nothing otherwise. */
    uint32_t              descriptors() const;

    /* too_large for less room than encoded_size; invalid_parameter for an
     * answer decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length, another magic, an operation or a
     * status this version does not have, a non-zero reserved byte, a body an
     * answer of its operation and status does not carry, or a descriptor
     * count other than descriptors(); not_supported for another major version.
     * Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    static constexpr uint32_t no_wire_status = 0xFFFFFFFFu;
    static constexpr uint8_t  on_pci_flag = 1u;

    bool     fields_valid() const;
    bool     body_empty() const;
    /* What `status` is on the wire, and back; no_wire_status and
     * invalid_parameter for one this version does not have. */
    uint32_t wire_status() const;
    bool     read_status(uint32_t wire);

    ExportOperation operation_;
    Status          status_;
    uint64_t        surface_;
    uint64_t        tag_;
    ExportGreeting  greeting_;
    ExportedLayout  layout_;
    uint64_t        writes_;
    uint32_t        reason_;
};

}  // namespace av

#endif  // AV_EXPORT_ANSWER_HPP
