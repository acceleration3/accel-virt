#ifndef AV_D3D_PIPELINE_LIBRARY_BLOB_HPP
#define AV_D3D_PIPELINE_LIBRARY_BLOB_HPP

#include "icd/d3d/hresult.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class PipelineLibraryBlob {
public:
    static constexpr uint32_t magic = 0x4C505641u;
    static constexpr uint32_t version = 1u;
    static constexpr size_t   header_bytes = 8u;

    PipelineLibraryBlob();
    PipelineLibraryBlob(const PipelineLibraryBlob &) = delete;
    PipelineLibraryBlob &operator=(const PipelineLibraryBlob &) = delete;

    size_t  size() const;
    Hresult opened(const void *data, size_t bytes) const;
    void    write(void *blob) const;

private:
    uint8_t header_[header_bytes];
};

}

#endif
