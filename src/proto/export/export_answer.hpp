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
    static constexpr uint32_t magic = 0x41585641u;
    static constexpr uint16_t major = 1u;
    static constexpr uint16_t minor = 5u;

    ExportAnswer();
    explicit ExportAnswer(Status status);
    ExportAnswer(const ExportRequest &request, Status status);
    ExportAnswer(ExportRevocation reason, uint64_t surface);

    void greet(const ExportGreeting &greeting);
    void describe(const ExportedLayout &layout);
    void count(uint64_t writes);
    void tell_refresh(uint32_t microhz);

    ExportOperation       operation() const;
    Status                status() const;
    uint64_t              surface() const;
    uint64_t              tag() const;
    const ExportGreeting &greeting() const;
    const ExportedLayout &layout() const;
    uint64_t              writes() const;
    uint32_t              refresh_microhz() const;
    uint32_t              reason() const;
    uint32_t              descriptors() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    static constexpr uint32_t no_wire_status = 0xFFFFFFFFu;
    static constexpr uint8_t  on_pci_flag = 1u;

    bool     fields_valid() const;
    bool     body_empty() const;
    uint32_t wire_status() const;
    bool     read_status(uint32_t wire);

    ExportOperation operation_;
    Status          status_;
    uint64_t        surface_;
    uint64_t        tag_;
    ExportGreeting  greeting_;
    ExportedLayout  layout_;
    uint64_t        writes_;
    uint32_t        refresh_microhz_;
    uint32_t        reason_;
};

}

#endif
