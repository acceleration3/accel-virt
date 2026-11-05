#ifndef AV_AV_REQUEST_WRITER_HPP
#define AV_AV_REQUEST_WRITER_HPP

#include "core/support/status.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextSetup;
class GuestPageList;
class ScanoutPlaneRequest;
class WireWriter;

class AvRequestWriter {
public:
    static constexpr size_t max_request_size = 96u;
    static constexpr size_t guest_blob_fixed_size = 56u;
    static constexpr size_t guest_blob_entry_size = 16u;
    static constexpr uint32_t name_bytes = 64u;

    AvRequestWriter(uint8_t *bytes, size_t capacity);

    [[nodiscard]] Status capset_info(uint32_t index);
    [[nodiscard]] Status capset(uint32_t capset_id, uint32_t version);
    [[nodiscard]] Status create_context(uint32_t context_id, uint32_t capset_id, const char *name, uint32_t name_length);
    [[nodiscard]] Status destroy_context(uint32_t context_id);
    [[nodiscard]] Status guest_blob(uint32_t resource_id, uint64_t address, uint32_t length);
    [[nodiscard]] Status guest_blob_pages(uint32_t resource_id, const GuestPageList &pages);
    [[nodiscard]] Status host_blob(uint32_t context_id, uint32_t resource_id, HostMemoryRequest memory, uint64_t size);
    [[nodiscard]] Status adopted_blob(uint32_t context_id, uint32_t resource_id, HostMemoryRequest memory,
                                      uint64_t size);
    [[nodiscard]] Status map_blob(uint32_t resource_id, uint64_t offset);
    [[nodiscard]] Status unmap_blob(uint32_t resource_id);
    [[nodiscard]] Status unref(uint32_t resource_id);
    [[nodiscard]] Status release_shared(uint64_t object_id);
    [[nodiscard]] Status display_modes();
    [[nodiscard]] Status display_clock(uint64_t address);
    [[nodiscard]] Status present_engine(uint64_t address);
    [[nodiscard]] Status window_stats(uint64_t address);
    [[nodiscard]] Status display_timing(uint32_t head, uint32_t refresh_millihz);
    [[nodiscard]] Status scanout_plane(const ScanoutPlaneRequest &plane);
    [[nodiscard]] Status setup(uint32_t context_id, const ContextSetup &setup);
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
    static constexpr uint32_t blob_memory_guest = 1u;
    static constexpr uint32_t blob_memory_host3d = 2u;
    static constexpr uint32_t blob_memory_host3d_guest = 3u;
    static constexpr uint32_t blob_flag_mappable = 1u;

    bool begin(WireWriter &writer, uint32_t type, uint32_t context_id, size_t size);
    [[nodiscard]] Status page(uint32_t type, uint64_t address);

    uint8_t *bytes_;
    size_t   capacity_;
    size_t   length_;
};

}

#endif
