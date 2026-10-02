/* The present engine's two records over attacker-chosen bytes: a queued
 * present as a hostile guest writes it, and the page as a hostile device
 * does. Whatever either decoder accepts is what the bytes at the documented
 * offsets say, holds every rule the protocol states, and encodes back to the
 * same bytes; a refusal leaves the decoder as it was. */
#include "core/support/status.hpp"
#include "proto/dma/present_operation.hpp"
#include "proto/pages/present_progress.hpp"
#include "proto/virtio_gpu/queued_present_request.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t header_size = 24u;

void check_request(const std::uint8_t *data, std::size_t size)
{
    av::WireReader           reader(data, size);
    av::QueuedPresentRequest request;
    if (request.decode(reader) != av::Status::ok) {
        if (request.ticket() != 0u || request.key() != 0u || request.copy().regions() != 0u) __builtin_trap();
        return;
    }
    const av::PresentCopyRequest &copy = request.copy();
    const bool image = request.source() == av::QueuedPresentRequest::Source::image;
    if (reader.offset() != av::QueuedPresentRequest::encoded_size || request.ticket() == 0u) __builtin_trap();
    if (image != (request.key() != 0u)) __builtin_trap();
    if (image && (copy.operation() != av::PresentOperation::blt || copy.source_entries() != 0u)) __builtin_trap();
    if (copy.regions() == 0u || copy.regions() > av::QueuedPresentRequest::max_regions) __builtin_trap();
    if (request.size() <= request.regions_offset() || request.regions_offset() < request.destination_entries_offset())
        __builtin_trap();
    std::uint8_t again[av::QueuedPresentRequest::encoded_size] = {};
    av::WireWriter writer(again, sizeof again);
    request.write(writer);
    if (writer.status() != av::Status::ok) __builtin_trap();
    /* The header is the dispatcher's, which chose this decoder by its type. */
    if (std::memcmp(again + header_size, data + header_size, sizeof again - header_size) != 0) __builtin_trap();
}

void check_progress(const std::uint8_t *data, std::size_t size)
{
    av::PresentProgress progress(7u, 3u);
    if (progress.decode(data, size) != av::Status::ok) {
        if (progress.completed() != 7u || progress.refused() != 3u) __builtin_trap();
        return;
    }
    if (progress.refused() > progress.completed()) __builtin_trap();
    std::uint8_t again[av::PresentProgress::encoded_size] = {};
    if (progress.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    check_request(data, size);
    check_progress(data, size);
    return 0;
}
