#include "core/support/status.hpp"
#include "proto/virtio_gpu/flush_request.hpp"
#include "proto/virtio_gpu/gpu_rect.hpp"
#include "proto/virtio_gpu/set_scanout_blob_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

std::uint32_t word(const std::uint8_t *data, std::size_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, data + offset, 4u);
    return value;
}

void refused_holds_nothing(const av::GpuRect &rect)
{
    if (rect.decoded() || rect.x() != 0u || rect.y() != 0u || rect.width() != 0u || rect.height() != 0u)
        __builtin_trap();
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader            scanout_reader(data, size);
    av::SetScanoutBlobRequest scanout;
    const bool                scanned = scanout.decode(scanout_reader) == av::Status::ok;
    if (scanned) {
        if (size < av::SetScanoutBlobRequest::encoded_size ||
            scanout_reader.offset() != av::SetScanoutBlobRequest::encoded_size)
            __builtin_trap();
        if (scanout.rect().x() != word(data, 24u) || scanout.rect().y() != word(data, 28u) ||
            scanout.rect().width() != word(data, 32u) || scanout.rect().height() != word(data, 36u) ||
            scanout.scanout_id() != word(data, 40u) || scanout.resource_id() != word(data, 44u) ||
            scanout.width() != word(data, 48u) || scanout.height() != word(data, 52u) ||
            scanout.format() != word(data, 56u) || scanout.stride() != word(data, 64u) ||
            scanout.offset() != word(data, 80u))
            __builtin_trap();
        if (word(data, 60u) != 0u) __builtin_trap();
        for (std::size_t plane = 1u; plane < av::SetScanoutBlobRequest::planes; plane++)
            if (word(data, 64u + plane * 4u) != 0u || word(data, 80u + plane * 4u) != 0u) __builtin_trap();
    } else if (scanout.decoded() || scanout.resource_id() != 0u || scanout.width() != 0u ||
               scanout.height() != 0u || scanout.format() != 0u || scanout.stride() != 0u ||
               scanout.offset() != 0u || scanout.scanout_id() != 0u) {
        __builtin_trap();
    } else {
        refused_holds_nothing(scanout.rect());
    }

    av::WireReader   flush_reader(data, size);
    av::FlushRequest flush;
    const bool       flushed = flush.decode(flush_reader) == av::Status::ok;
    if (flushed) {
        if (size < av::FlushRequest::encoded_size || flush_reader.offset() != av::FlushRequest::encoded_size)
            __builtin_trap();
        if (flush.resource_id() != word(data, 40u) || word(data, 44u) != 0u ||
            flush.rect().width() != word(data, 32u))
            __builtin_trap();
    } else if (flush.decoded() || flush.resource_id() != 0u) {
        __builtin_trap();
    } else {
        refused_holds_nothing(flush.rect());
    }
    return 0;
}
