#include "icd/d3d/pipeline_library_blob.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

namespace {

constexpr size_t word_bytes = 4u;
constexpr uint32_t byte_bits = 8u;

void put_word(uint8_t *into, uint32_t word)
{
    AV_SCOPE("put_word");
    for (size_t byte = 0u; byte < word_bytes; byte++) into[byte] = static_cast<uint8_t>(word >> (byte * byte_bits));
}

}  // namespace

PipelineLibraryBlob::PipelineLibraryBlob() : header_{}
{
    AV_SCOPE("PipelineLibraryBlob::PipelineLibraryBlob");
    put_word(header_, magic);
    put_word(header_ + word_bytes, version);
}

size_t PipelineLibraryBlob::size() const
{
    AV_SCOPE("PipelineLibraryBlob::size");
    return header_bytes;
}

Hresult PipelineLibraryBlob::opened(const void *data, size_t bytes) const
{
    AV_SCOPE("PipelineLibraryBlob::opened");
    if (bytes == 0u) return hr_ok;
    if (data == nullptr || bytes != header_bytes) return hr_d3d12_driver_version_mismatch;
    const uint8_t *const given = static_cast<const uint8_t *>(data);
    for (size_t byte = 0u; byte < header_bytes; byte++)
        if (given[byte] != header_[byte]) return hr_d3d12_driver_version_mismatch;
    return hr_ok;
}

void PipelineLibraryBlob::write(void *blob) const
{
    AV_SCOPE("PipelineLibraryBlob::write");
    uint8_t *const into = static_cast<uint8_t *>(blob);
    for (size_t byte = 0u; byte < header_bytes; byte++) into[byte] = header_[byte];
}

}  // namespace av::d3d
