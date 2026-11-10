#include "icd/d3d/kernel_heap.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/shared_bytes.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/surface_layout.hpp"

namespace av::d3d {

KernelHeap::KernelHeap() : allocation_(), resource_{}, bytes_(0u), host_object_(0u), cpu_visible_(false)
{
    AV_SCOPE("KernelHeap::KernelHeap");
}

KernelHeap::KernelHeap(uint64_t bytes, bool cpu_visible, uint64_t host_object)
    : allocation_(), resource_{}, bytes_(SharedBytes(bytes).pages()), host_object_(host_object),
      cpu_visible_(cpu_visible)
{
    AV_SCOPE("KernelHeap::KernelHeap");
}

Status KernelHeap::make()
{
    AV_SCOPE("KernelHeap::make");
    if (bytes_ == 0u) return Status::invalid_parameter;
    allocation_ = SharedAllocationData(
        AllocationDescriptor(AllocationKind::host_heap, cpu_visible_, SurfaceLayout(), bytes_, ScanoutTarget(),
                             host_object_),
        SharedObjectKind(SharedObjectKind::memory), bytes_);
    resource_[0] = mark;
    resource_[1] = 0u;
    return Status::ok;
}

Status KernelHeap::open(const void *resource, size_t resource_size, const void *allocation, size_t allocation_size)
{
    AV_SCOPE("KernelHeap::open");
    if (resource == nullptr || resource_size < resource_bytes) return Status::not_found;
    uint64_t marked[2] = {};
    __builtin_memcpy(marked, resource, resource_bytes);
    if (marked[0] != mark) return Status::not_found;
    SharedAllocationData read;
    if (read.decode(static_cast<const uint8_t *>(allocation), allocation_size) != Status::ok) return Status::malformed;
    const AllocationDescriptor &described = read.descriptor();
    if (described.kind() != AllocationKind::host_heap) return Status::not_found;
    __builtin_memcpy(resource_, marked, resource_bytes);
    allocation_ = read;
    bytes_ = described.size();
    cpu_visible_ = described.cpu_visible();
    host_object_ = described.host_key();
    return Status::ok;
}

uint8_t *KernelHeap::allocation_data()
{
    AV_SCOPE("KernelHeap::allocation_data");
    return allocation_.lend();
}

const uint64_t *KernelHeap::resource_data() const
{
    AV_SCOPE("KernelHeap::resource_data");
    return resource_;
}

uint64_t KernelHeap::bytes() const
{
    AV_SCOPE("KernelHeap::bytes");
    return bytes_;
}

bool KernelHeap::cpu_visible() const
{
    AV_SCOPE("KernelHeap::cpu_visible");
    return cpu_visible_;
}

uint64_t KernelHeap::host_object() const
{
    AV_SCOPE("KernelHeap::host_object");
    return host_object_;
}

uint64_t KernelHeap::key() const
{
    AV_SCOPE("KernelHeap::key");
    SharedAllocationData minted;
    if (minted.decode(allocation_.bytes(), allocation_bytes) != Status::ok) return 0u;
    return minted.object_id();
}

}
