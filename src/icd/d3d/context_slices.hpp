#ifndef AV_D3D_CONTEXT_SLICES_HPP
#define AV_D3D_CONTEXT_SLICES_HPP

#include "icd/d3d/buffer_slices.hpp"

#include <stdint.h>

namespace av::d3d {

class ContextSlices {
public:
    ContextSlices();
    ContextSlices(const ContextSlices &) = delete;
    ContextSlices &operator=(const ContextSlices &) = delete;

    void own(BufferSlices *slices);

    [[nodiscard]] bool place(uint64_t bytes, BufferSlices::Slice &slice);
    [[nodiscard]] bool rename(uint64_t bytes, BufferSlices::Slice &slice);
    void               leave(BufferSlices::Slice &slice);
    void               trim();

    uint8_t *view(const BufferSlices::Slice &slice) const;
    uint64_t memory(const BufferSlices::Slice &slice) const;
    uint64_t offset_in_block(const BufferSlices::Slice &slice) const;

private:
    BufferSlices *slices_;
};

}

#endif
