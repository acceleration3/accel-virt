/* One control response the driver receives (virtio 1.2 5.7.6.7), read from the
 * bytes the device wrote: its type, and the payload OK_CAPSET_INFO and
 * OK_CAPSET carry. The driver judges the values; this checks the bytes. */
#ifndef AV_GPU_RESPONSE_READER_HPP
#define AV_GPU_RESPONSE_READER_HPP

#include "core/support/status.hpp"
#include "proto/context/capset_info.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GpuResponseReader {
public:
    GpuResponseReader();

    /* malformed when shorter than a header or than its type's payload, or for
     * a capset that does not decode. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    uint32_t             type() const;
    /* An OK_* answer. */
    bool                 ok() const;
    uint32_t             capset_id() const;
    uint32_t             capset_max_version() const;
    uint32_t             capset_max_size() const;
    const CapsetInfo    &capset() const;

private:
    static constexpr size_t   header_size = 24u;
    static constexpr uint32_t ok_capset_info = 0x1102u;
    static constexpr uint32_t ok_capset = 0x1103u;
    static constexpr uint32_t ok_range_first = 0x1100u;
    static constexpr uint32_t error_range_first = 0x1200u;

    uint32_t   type_;
    uint32_t   capset_id_;
    uint32_t   capset_max_version_;
    uint32_t   capset_max_size_;
    CapsetInfo capset_;
};

}  // namespace av

#endif  // AV_GPU_RESPONSE_READER_HPP
