#ifndef AV_D3D12_INDIRECT_TABLE_HPP
#define AV_D3D12_INDIRECT_TABLE_HPP

#include <stdint.h>

struct ID3D12Device;
struct ID3D12Resource;

namespace av {

class D3d12AddressMap;

class D3d12IndirectTable {
public:
    explicit D3d12IndirectTable(const D3d12AddressMap &addresses);
    ~D3d12IndirectTable();
    D3d12IndirectTable(const D3d12IndirectTable &) = delete;
    D3d12IndirectTable &operator=(const D3d12IndirectTable &) = delete;

    ID3D12Resource *current(ID3D12Device &device, uint64_t &address);
    void            forget();

private:
    const D3d12AddressMap *addresses_;
    ID3D12Resource        *resource_;
    uint64_t               address_;
    uint32_t               version_;
    bool                   made_;
};

}

#endif
