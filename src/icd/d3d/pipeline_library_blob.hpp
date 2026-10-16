/* What a Direct3D 12 pipeline library of this driver serializes to: a header
 * naming the driver and its format, and nothing else.
 *
 * The runtime keeps a library's names and hands the driver only an index, and
 * every pipeline it loads from one comes with its whole description - the
 * library reference is a hint a driver may use to skip a compile. The host
 * compiles every pipeline from that description, so there is nothing of
 * avumd's to cache: a library is accepted, filled and serialized, and its
 * blob only has to be recognised when an application hands it back. A blob
 * this format does not recognise is another driver's, and is refused the way
 * D3D12 tells an application to drop a stale cache. */
#ifndef AV_D3D_PIPELINE_LIBRARY_BLOB_HPP
#define AV_D3D_PIPELINE_LIBRARY_BLOB_HPP

#include "icd/d3d/hresult.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class PipelineLibraryBlob {
public:
    /* "AVPL", then the format's version. */
    static constexpr uint32_t magic = 0x4C505641u;
    static constexpr uint32_t version = 1u;
    static constexpr size_t   header_bytes = 8u;

    PipelineLibraryBlob();
    PipelineLibraryBlob(const PipelineLibraryBlob &) = delete;
    PipelineLibraryBlob &operator=(const PipelineLibraryBlob &) = delete;

    /* What a serialized library takes: the header. */
    size_t  size() const;
    /* CreatePipelineLibrary's answer for `bytes` of initial data: hr_ok for
     * none (a new library) or this format's header, else
     * D3D12_ERROR_DRIVER_VERSION_MISMATCH. */
    Hresult opened(const void *data, size_t bytes) const;
    /* The header into `blob`, which holds size() bytes. */
    void    write(void *blob) const;

private:
    uint8_t header_[header_bytes];
};

}  // namespace av::d3d

#endif  // AV_D3D_PIPELINE_LIBRARY_BLOB_HPP
