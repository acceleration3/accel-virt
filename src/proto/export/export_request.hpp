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
    static constexpr uint32_t magic = 0x51585641u;
    static constexpr uint16_t major = 1u;
    static constexpr uint16_t minor = 5u;
    static constexpr uint64_t most_heads = 16u;
    static constexpr uint32_t other_gpu = 1u;
    static constexpr uint32_t least_refresh_microhz = 1000000u;
    static constexpr uint32_t most_refresh_microhz = 1000000000u;
    static constexpr int64_t  most_shift_ns = 1000000000;

    ExportRequest();
    ExportRequest(ExportOperation operation, uint64_t surface, uint64_t tag);
    ExportRequest(ExportOperation operation, uint64_t surface, uint64_t tag, uint32_t flags);
    ExportRequest(int64_t shift_ns, uint64_t tag);

    ExportOperation operation() const;
    uint64_t        surface() const;
    uint32_t        head() const;
    uint32_t        flags() const;
    bool            imports_on_other_gpu() const;
    uint64_t        tag() const;
    int64_t         shift_ns() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    bool fields_valid() const;

    ExportOperation operation_;
    uint64_t        surface_;
    uint64_t        tag_;
    int64_t         shift_ns_;
    uint32_t        flags_;
};

}

#endif
