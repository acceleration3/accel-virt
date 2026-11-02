#ifndef AV_SHARED_ALLOCATION_DATA_HPP
#define AV_SHARED_ALLOCATION_DATA_HPP

#include "core/support/status.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SharedAllocationData {
public:
    static constexpr size_t encoded_size = AllocationDescriptor::encoded_size + SharedObjectRecord::encoded_size;

    SharedAllocationData();
    SharedAllocationData(const AllocationDescriptor &descriptor, const SharedObjectKind &kind, uint64_t size);

    const uint8_t *bytes() const;
    uint8_t       *lend();
    [[nodiscard]] Status reread(size_t written);
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    [[nodiscard]] Status mint(const SharedObjectRecord &minted);

    bool                        read() const;
    const AllocationDescriptor &descriptor() const;
    const SharedObjectRecord   &record() const;
    bool                        assigned() const;
    uint64_t                    object_id() const;
    uint64_t                    size() const;

    bool invariants_hold() const;

private:
    uint8_t              bytes_[encoded_size];
    AllocationDescriptor descriptor_;
    SharedObjectRecord   record_;
    bool                 read_;
};

}

#endif
