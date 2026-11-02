#ifndef AV_CREATE_BLOB_REQUEST_HPP
#define AV_CREATE_BLOB_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class CreateBlobRequest {
public:
    static constexpr size_t   fixed_size = 56u;
    static constexpr size_t   entry_size = 16u;
    static constexpr uint32_t memory_guest = 1u;
    static constexpr uint32_t memory_host3d = 2u;
    static constexpr uint32_t memory_host3d_guest = 3u;
    static constexpr uint32_t flag_mappable = 1u;
    static constexpr uint32_t flag_shareable = 2u;
    static constexpr uint32_t flag_cross_device = 4u;

    CreateBlobRequest();

    [[nodiscard]] Status decode(WireReader &reader);
    uint32_t             context_id() const;
    uint32_t             resource_id() const;
    uint32_t             memory() const;
    uint32_t             flags() const;
    uint32_t             entries() const;
    uint64_t             blob_id() const;
    uint64_t             size() const;

private:
    static constexpr size_t header_before_context = 16u;
    static constexpr size_t header_after_context = 4u;

    uint32_t context_id_;
    uint32_t resource_id_;
    uint32_t memory_;
    uint32_t flags_;
    uint32_t entries_;
    uint64_t blob_id_;
    uint64_t size_;
};

}

#endif
