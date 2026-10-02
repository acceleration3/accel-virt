/* The stride a command signature was made with, kept on it as private data:
 * what ExecuteIndirect12 bounds an argument buffer by, since vkd3d-proton's
 * signature keeps its description to itself. */
#ifndef AV_D3D12_INDIRECT_STRIDE_HPP
#define AV_D3D12_INDIRECT_STRIDE_HPP

#include <stdint.h>

struct ID3D12CommandSignature;

namespace av {

class D3d12IndirectStride {
public:
    /* None: what read() fills. */
    D3d12IndirectStride();
    explicit D3d12IndirectStride(uint32_t stride);

    [[nodiscard]] bool stamp(ID3D12CommandSignature *signature) const;
    /* False for a signature that carries none. */
    [[nodiscard]] bool read(ID3D12CommandSignature *signature);
    /* Whether `count` commands from `offset` fit a buffer of `width` bytes,
     * `offset` a whole number of the four-byte words indirect arguments are. */
    bool reaches(uint64_t offset, uint32_t count, uint64_t width) const;

private:
    static constexpr uint64_t word_bytes = 4u;

    uint32_t stride_;
};

}  // namespace av

#endif  // AV_D3D12_INDIRECT_STRIDE_HPP
