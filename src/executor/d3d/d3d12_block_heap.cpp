#include "executor/d3d/d3d12_block_heap.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/source_memory.hpp"

namespace av {

D3d12BlockHeap::D3d12BlockHeap(SourceMemory &store)
    : store_(&store), heap_(nullptr), heaps_(0u), resources_(0u), mark_(0u), abandoned_(false)
{
    AV_SCOPE("D3d12BlockHeap::D3d12BlockHeap");
    store.acquire();
}

D3d12BlockHeap::~D3d12BlockHeap()
{
    AV_SCOPE("D3d12BlockHeap::~D3d12BlockHeap");
    if (heap_ != nullptr) heap_->Release();
    store_->owner()->release(*store_);
}

bool D3d12BlockHeap::import(const D3d12HostDevice &device)
{
    AV_SCOPE("D3d12BlockHeap::import");
    if (heap_ != nullptr) return true;
    OwnedFd descriptor;
    if (!store_->share(descriptor)) return false;
    heap_ = device.import_dmabuf_heap(store_->pointer(), descriptor.fd(), store_->size());
    return heap_ != nullptr;
}

void D3d12BlockHeap::took_heap()
{
    AV_SCOPE("D3d12BlockHeap::took_heap");
    heaps_++;
}

void D3d12BlockHeap::let_go_heap(uint64_t mark)
{
    AV_SCOPE("D3d12BlockHeap::let_go_heap");
    if (heaps_ != 0u) heaps_--;
    used_at(mark);
}

void D3d12BlockHeap::took_resource()
{
    AV_SCOPE("D3d12BlockHeap::took_resource");
    resources_++;
}

void D3d12BlockHeap::let_go_resource(uint64_t mark)
{
    AV_SCOPE("D3d12BlockHeap::let_go_resource");
    if (resources_ != 0u) resources_--;
    used_at(mark);
}

void D3d12BlockHeap::abandon(uint64_t mark)
{
    AV_SCOPE("D3d12BlockHeap::abandon");
    abandoned_ = true;
    used_at(mark);
}

void D3d12BlockHeap::used_at(uint64_t mark)
{
    AV_SCOPE("D3d12BlockHeap::used_at");
    if (mark > mark_) mark_ = mark;
}

bool D3d12BlockHeap::holds(const SourceMemory &store) const
{
    AV_SCOPE("D3d12BlockHeap::holds");
    return store_ == &store;
}

ID3D12Heap *D3d12BlockHeap::heap() const
{
    AV_SCOPE("D3d12BlockHeap::heap");
    return heap_;
}

uint64_t D3d12BlockHeap::bytes() const
{
    AV_SCOPE("D3d12BlockHeap::bytes");
    return store_->size();
}

bool D3d12BlockHeap::abandoned() const
{
    AV_SCOPE("D3d12BlockHeap::abandoned");
    return abandoned_;
}

bool D3d12BlockHeap::unreferenced() const
{
    AV_SCOPE("D3d12BlockHeap::unreferenced");
    return heaps_ == 0u && resources_ == 0u;
}

bool D3d12BlockHeap::done(uint64_t reached) const
{
    AV_SCOPE("D3d12BlockHeap::done");
    return abandoned_ && unreferenced() && mark_ <= reached;
}

bool D3d12BlockHeap::invariants_hold() const
{
    AV_SCOPE("D3d12BlockHeap::invariants_hold");
    return store_ != nullptr && (heap_ != nullptr || (heaps_ == 0u && resources_ == 0u));
}

}
