#ifndef AV_SHARED_OBJECT_REQUEST_HPP
#define AV_SHARED_OBJECT_REQUEST_HPP

#include "core/support/status.hpp"
#include "proto/resources/shared_object_kind.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SharedObjectRequest {
public:
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint64_t page_size = SharedObjectKind::page_size;

    SharedObjectRequest();
    SharedObjectRequest(uint64_t object_id, const SharedObjectKind &kind, uint64_t wire_handle,
                        uint64_t size);

    uint64_t                object_id() const;
    const SharedObjectKind &kind() const;
    uint64_t                wire_handle() const;
    uint64_t                size() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    uint64_t         object_id_;
    SharedObjectKind kind_;
    uint64_t         wire_handle_;
    uint64_t         size_;
};

}

#endif
