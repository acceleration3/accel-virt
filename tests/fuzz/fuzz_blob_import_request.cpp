#include "core/support/status.hpp"
#include "proto/submission/blob_import_request.hpp"
#include "proto/submission/blob_role.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::BlobImportRequest request;
    const av::Status      decoded = request.decode(data, size);
    if (decoded == av::Status::malformed) {
        require(size != av::BlobImportRequest::encoded_size);
        return 0;
    }
    require(size == av::BlobImportRequest::encoded_size);
    if (decoded != av::Status::ok) {
        require(request.resource_id() == 0u && request.role() == av::BlobRole());
        return 0;
    }
    require(request.resource_id() != 0u && request.role().invariants_hold());
    std::uint8_t again[av::BlobImportRequest::encoded_size] = {};
    require(request.encode(again, sizeof again) == av::Status::ok);
    require(std::memcmp(again, data, size) == 0);
    return 0;
}
