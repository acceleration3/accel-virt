#ifndef AV_D3D12_HEAP_FLAGS_HPP
#define AV_D3D12_HEAP_FLAGS_HPP

#include <stdint.h>

namespace av {

class D3d12HeapFlags {
public:
    explicit D3d12HeapFlags(uint32_t guest);

    uint32_t kept() const;
    bool     refused() const;

private:
    uint32_t guest_;
};

}

#endif
