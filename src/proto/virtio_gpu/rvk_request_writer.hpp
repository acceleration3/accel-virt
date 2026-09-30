/* The virtio-gpu requests a driver sends to use rvk contexts (virtio 1.2
 * 5.7.6, docs/protocol.md): the capset queries, context creation and
 * destruction, a guest blob of one entry and its release, and the context
 * setup carried by SUBMIT_3D. Each request replaces what the destination held;
 * one that does not fit writes nothing. The device's decoders read them. */
#ifndef AV_RVK_REQUEST_WRITER_HPP
#define AV_RVK_REQUEST_WRITER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextSetup;
class GuestPageList;
class WireWriter;

class RvkRequestWriter {
public:
    static constexpr size_t max_request_size = 96u;
    /* What a create-blob request costs before its memory entries, and what
     * each entry costs: a caller sizing a buffer for one over N pages needs
     * both (feature 058's scanout blob). */
    static constexpr size_t guest_blob_fixed_size = 56u;
    static constexpr size_t guest_blob_entry_size = 16u;

    RvkRequestWriter(uint8_t *bytes, size_t capacity);

    [[nodiscard]] Status capset_info(uint32_t index);
    [[nodiscard]] Status capset(uint32_t capset_id, uint32_t version);
    /* No debug name. */
    [[nodiscard]] Status create_context(uint32_t context_id, uint32_t capset_id);
    [[nodiscard]] Status destroy_context(uint32_t context_id);
    /* Guest memory at `address` of `length` bytes, one entry covering the blob. */
    [[nodiscard]] Status guest_blob(uint32_t resource_id, uint64_t address, uint32_t length);
    /* The scattered guest pages of an aperture allocation as one GUEST blob,
     * one memory entry per run of adjacent pages, its size their total - what a
     * ring, fence or reply is. The device already decodes many entries; this is
     * the writer catching up, mirroring attach_backing_pages. */
    [[nodiscard]] Status guest_blob_pages(uint32_t resource_id, const GuestPageList &pages);
    /* Memory of the host's, mappable into the hostmem region: no memory entries,
     * a page-multiple size, and nothing of the guest behind it. */
    [[nodiscard]] Status host_blob(uint32_t resource_id, uint64_t size);
    /* Where in the hostmem region the device is to place it: the offset is the
     * guest's to choose, and the device refuses one that does not fit
     * (docs/protocol.md). */
    [[nodiscard]] Status map_blob(uint32_t resource_id, uint64_t offset);
    [[nodiscard]] Status unmap_blob(uint32_t resource_id);
    [[nodiscard]] Status unref(uint32_t resource_id);
    /* The shared object `object_id` is gone from the guest: every process let
     * go of the allocation it was minted over (SharedReleaseRequest). */
    [[nodiscard]] Status release_shared(uint64_t object_id);
    /* GET_DISPLAY_MODES: the modes the host's display offers (DisplayModeSet). */
    [[nodiscard]] Status display_modes();
    /* DISPLAY_CLOCK: the page the device publishes vertical blanks in, 0 for
     * none (DisplayClockRequest). */
    [[nodiscard]] Status display_clock(uint64_t address);
    /* DISPLAY_TIMING: head `head` blanks at `refresh_millihz` (DisplayTimingRequest). */
    [[nodiscard]] Status display_timing(uint32_t head, uint32_t refresh_millihz);
    [[nodiscard]] Status setup(uint32_t context_id, const ContextSetup &setup);
    /* Bytes of the last request written; zero after one that did not fit. */
    size_t               length() const;

private:
    enum : uint32_t {
        get_capset_info = 0x0108u,
        get_capset = 0x0109u,
        resource_unref = 0x0102u,
        resource_create_blob = 0x010Cu,
        resource_map_blob = 0x0208u,
        resource_unmap_blob = 0x0209u,
        ctx_create = 0x0200u,
        ctx_destroy = 0x0201u,
        submit_3d = 0x0207u,
    };
    static constexpr size_t   header_size = 24u;
    static constexpr size_t   short_request_size = 32u;
    static constexpr size_t   create_context_size = 96u;
    static constexpr size_t   guest_blob_size = guest_blob_fixed_size + guest_blob_entry_size;
    static constexpr size_t   host_blob_size = 56u;
    static constexpr size_t   map_blob_size = 40u;
    static constexpr uint32_t name_bytes = 64u;
    static constexpr uint32_t blob_memory_guest = 1u;
    static constexpr uint32_t blob_memory_host3d = 2u;
    static constexpr uint32_t blob_flag_mappable = 1u;

    /* Starts a request of `size` bytes with its header; false when it cannot fit. */
    bool begin(WireWriter &writer, uint32_t type, uint32_t context_id, size_t size);

    uint8_t *bytes_;
    size_t   capacity_;
    size_t   length_;
};

}  // namespace av

#endif  // AV_RVK_REQUEST_WRITER_HPP
