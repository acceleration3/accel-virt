#ifndef AV_D3D_CONTEXT_SLICES_HPP
#define AV_D3D_CONTEXT_SLICES_HPP

#include "icd/common/stream_memory.hpp"

#include <stdint.h>

namespace av::d3d {

class ContextSlices {
public:
    ContextSlices();
    ContextSlices(const ContextSlices &) = delete;
    ContextSlices &operator=(const ContextSlices &) = delete;

    void own(icd::StreamMemory *slices);

    [[nodiscard]] bool place(uint64_t bytes, icd::StreamMemory::Slice &slice);
    [[nodiscard]] bool rename(uint64_t bytes, icd::StreamMemory::Slice &slice);
    void               leave(icd::StreamMemory::Slice &slice);
    void               trim();

    bool     available() const;
    uint8_t *view(const icd::StreamMemory::Slice &slice) const;
    uint64_t memory(const icd::StreamMemory::Slice &slice) const;
    uint64_t offset_in_block(const icd::StreamMemory::Slice &slice) const;

private:
    icd::StreamMemory *slices_;
};

}

#endif
