#include "executor/d3d/slice_storage.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/source_memory.hpp"

namespace av {

SliceStorage::SliceStorage(SourceMemory &store) : store_(&store), buffer_(), mark_(0u), refused_(false), retired_(false)
{
    AV_SCOPE("SliceStorage::SliceStorage");
    store.acquire();
}

SliceStorage::~SliceStorage()
{
    AV_SCOPE("SliceStorage::~SliceStorage");
    buffer_.release();
    store_->owner()->release(*store_);
}

bool SliceStorage::import(D3d11HostDevice &device)
{
    AV_SCOPE("SliceStorage::import");
    OwnedFd descriptor;
    if (store_->share(descriptor) && device.import_to_draw(descriptor, store_->size(), buffer_)) return true;
    refused_ = true;
    return false;
}

void SliceStorage::used_at(uint64_t mark)
{
    AV_SCOPE("SliceStorage::used_at");
    mark_ = mark;
}

void SliceStorage::retire()
{
    AV_SCOPE("SliceStorage::retire");
    retired_ = true;
}

bool SliceStorage::holds(const SourceMemory &store) const
{
    AV_SCOPE("SliceStorage::holds");
    return store_ == &store;
}

bool SliceStorage::imported() const
{
    AV_SCOPE("SliceStorage::imported");
    return buffer_.made();
}

bool SliceStorage::refused() const
{
    AV_SCOPE("SliceStorage::refused");
    return refused_;
}

bool SliceStorage::retired() const
{
    AV_SCOPE("SliceStorage::retired");
    return retired_;
}

bool SliceStorage::done(uint64_t reached) const
{
    AV_SCOPE("SliceStorage::done");
    return retired_ && mark_ <= reached;
}

const VulkanBuffer &SliceStorage::buffer() const
{
    AV_SCOPE("SliceStorage::buffer");
    return buffer_;
}

}
