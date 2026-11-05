#ifndef AV_D3D12_BLOCK_HEAP_HPP
#define AV_D3D12_BLOCK_HEAP_HPP

#include <stdint.h>

struct ID3D12Heap;

namespace av {

class D3d12HostDevice;
class SourceMemory;

class D3d12BlockHeap {
public:
    explicit D3d12BlockHeap(SourceMemory &store);
    ~D3d12BlockHeap();
    D3d12BlockHeap(const D3d12BlockHeap &) = delete;
    D3d12BlockHeap &operator=(const D3d12BlockHeap &) = delete;

    [[nodiscard]] bool import(const D3d12HostDevice &device);
    void               took_heap();
    void               let_go_heap(uint64_t mark);
    void               took_resource();
    void               let_go_resource(uint64_t mark);
    void               abandon(uint64_t mark);

    bool        holds(const SourceMemory &store) const;
    ID3D12Heap *heap() const;
    uint64_t    bytes() const;
    bool        abandoned() const;
    bool        unreferenced() const;
    bool        done(uint64_t reached) const;

    bool invariants_hold() const;

private:
    void used_at(uint64_t mark);

    SourceMemory *store_;
    ID3D12Heap   *heap_;
    uint32_t      heaps_;
    uint32_t      resources_;
    uint64_t      mark_;
    bool          abandoned_;
};

}

#endif
