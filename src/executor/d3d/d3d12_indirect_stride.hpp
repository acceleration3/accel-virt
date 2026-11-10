#ifndef AV_D3D12_INDIRECT_STRIDE_HPP
#define AV_D3D12_INDIRECT_STRIDE_HPP

#include <stdint.h>

struct ID3D12CommandSignature;

namespace av {

class D3d12IndirectStride {
public:
    D3d12IndirectStride();
    D3d12IndirectStride(uint32_t stride, bool addressed);
    explicit D3d12IndirectStride(uint32_t stride);

    [[nodiscard]] bool stamp(ID3D12CommandSignature *signature) const;
    [[nodiscard]] bool read(ID3D12CommandSignature *signature);
    bool addressed() const;
    bool reaches(uint64_t offset, uint32_t count, uint64_t width) const;

private:
    static constexpr uint64_t word_bytes = 4u;

    uint32_t stride_;
    bool     addressed_;
};

}

#endif
