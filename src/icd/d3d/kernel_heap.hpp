#ifndef AV_KERNEL_HEAP_HPP
#define AV_KERNEL_HEAP_HPP

#include "core/support/status.hpp"
#include "proto/resources/shared_allocation_data.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class KernelHeap {
public:
    static constexpr size_t allocation_bytes = SharedAllocationData::encoded_size;
    static constexpr size_t resource_bytes = 2u * sizeof(uint64_t);

    KernelHeap();
    KernelHeap(uint64_t bytes, bool cpu_visible, uint64_t host_object);
    KernelHeap(const KernelHeap &) = delete;
    KernelHeap &operator=(const KernelHeap &) = delete;

    [[nodiscard]] Status make();
    [[nodiscard]] Status open(const void *resource, size_t resource_size, const void *allocation,
                              size_t allocation_size);

    uint8_t        *allocation_data();
    const uint64_t *resource_data() const;
    uint64_t        bytes() const;
    bool            cpu_visible() const;
    uint64_t        host_object() const;
    uint64_t        key() const;

private:
    static constexpr uint64_t mark = 0x50414548u;

    SharedAllocationData allocation_;
    uint64_t             resource_[2];
    uint64_t             bytes_;
    uint64_t             host_object_;
    bool                 cpu_visible_;
};

}

#endif
