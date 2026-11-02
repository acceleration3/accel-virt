#ifndef AV_SHARED_OBJECT_RECORD_HPP
#define AV_SHARED_OBJECT_RECORD_HPP

#include "core/support/status.hpp"
#include "proto/resources/shared_object_kind.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SharedObjectRecord {
public:
    static constexpr size_t   encoded_size = 40u;
    static constexpr uint64_t page_size = SharedObjectKind::page_size;

    SharedObjectRecord();
    SharedObjectRecord(uint64_t object_id, const SharedObjectKind &kind, uint64_t adapter_id, uint64_t size);

    uint64_t                object_id() const;
    const SharedObjectKind &kind() const;
    uint64_t                adapter_id() const;
    uint64_t                size() const;
    bool                    assigned() const;
    void                    assign(uint64_t object_id, uint64_t adapter_id);

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00535641u;
    static constexpr uint32_t version = 1u;

    bool fields_valid() const;

    uint64_t         object_id_;
    SharedObjectKind kind_;
    uint64_t         adapter_id_;
    uint64_t         size_;
};

}

#endif
