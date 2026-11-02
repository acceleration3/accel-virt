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

    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    uint32_t             type() const;
    bool                 ok() const;
    uint32_t             capset_id() const;
    uint32_t             capset_max_version() const;
    uint32_t             capset_max_size() const;
    uint32_t             map_info() const;
    const CapsetInfo    &capset() const;

private:
    static constexpr size_t   header_size = 24u;
    static constexpr uint32_t ok_capset_info = 0x1102u;
    static constexpr uint32_t ok_capset = 0x1103u;
    static constexpr uint32_t ok_map_info = 0x1106u;
    static constexpr uint32_t ok_range_first = 0x1100u;
    static constexpr uint32_t error_range_first = 0x1200u;

    uint32_t   type_;
    uint32_t   capset_id_;
    uint32_t   capset_max_version_;
    uint32_t   capset_max_size_;
    uint32_t   map_info_;
    CapsetInfo capset_;
};

}

#endif
