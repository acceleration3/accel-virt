#include "executor/d3d/d3d12_indirect_table.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_address_map.hpp"
#include "executor/gpu_range_table.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {
constexpr uint64_t least_bytes = 256u;
}

D3d12IndirectTable::D3d12IndirectTable(const D3d12AddressMap &addresses)
    : addresses_(&addresses), resource_(nullptr), address_(0u), version_(0u), made_(false)
{
    AV_SCOPE("D3d12IndirectTable::D3d12IndirectTable");
}

D3d12IndirectTable::~D3d12IndirectTable()
{
    AV_SCOPE("D3d12IndirectTable::~D3d12IndirectTable");
    forget();
}

void D3d12IndirectTable::forget()
{
    AV_SCOPE("D3d12IndirectTable::forget");
    if (resource_ != nullptr) resource_->Release();
    resource_ = nullptr;
    address_ = 0u;
    made_ = false;
}

ID3D12Resource *D3d12IndirectTable::current(ID3D12Device &device, uint64_t &address)
{
    AV_SCOPE("D3d12IndirectTable::current");
    address = 0u;
    if (made_ && version_ == addresses_->version()) {
        address = address_;
        return resource_;
    }
    forget();
    const uint32_t count = addresses_->count();
    const uint64_t bytes = GpuRangeTable(0u).bytes_for(count);
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes < least_bytes ? least_bytes : bytes;
    desc.Height = 1u;
    desc.DepthOrArraySize = 1u;
    desc.MipLevels = 1u;
    desc.SampleDesc.Count = 1u;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource *made = nullptr;
    if (FAILED(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                              nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void **>(&made))))
        return nullptr;
    const D3D12_RANGE none = {0u, 0u};
    void             *mapped = nullptr;
    const uint64_t    where = made->GetGPUVirtualAddress();
    if (FAILED(made->Map(0u, &none, &mapped)) || mapped == nullptr) {
        made->Release();
        return nullptr;
    }
    GpuRangeTable::Row *const rows = reinterpret_cast<GpuRangeTable::Row *>(static_cast<uint8_t *>(mapped) +
                                                                          GpuRangeTable::header_bytes);
    const uint32_t taken = addresses_->rows(rows, count);
    const bool     filled = taken == count && GpuRangeTable(where).fill(mapped, desc.Width, rows, taken);
    made->Unmap(0u, &none);
    if (!filled) {
        made->Release();
        return nullptr;
    }
    resource_ = made;
    address_ = where;
    version_ = addresses_->version();
    made_ = true;
    address = where;
    return resource_;
}

}
