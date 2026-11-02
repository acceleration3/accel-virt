#ifndef AV_HOST_VISIBLE_BLOB_HPP
#define AV_HOST_VISIBLE_BLOB_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <stdint.h>

namespace av {

class VirtioGpuDevice;
struct HostVisibleBlobProbe;

class HostVisibleBlob {
public:
    static constexpr uint64_t page_bytes = 0x1000u;
    static constexpr uint32_t pages_per_piece = 2048u;

    explicit HostVisibleBlob(Platform &platform);
    HostVisibleBlob(Platform &platform, uint32_t resource, uint64_t offset);
    HostVisibleBlob(Platform &platform, uint32_t resource);
    HostVisibleBlob(const HostVisibleBlob &) = delete;
    HostVisibleBlob &operator=(const HostVisibleBlob &) = delete;

    [[nodiscard]] Status create(VirtioGpuDevice &device, uint32_t context_id, uint32_t resource_id,
                                HostMemoryRequest memory, uint64_t bytes);
    [[nodiscard]] Status create_over(VirtioGpuDevice &device, uint32_t context_id, uint32_t resource_id,
                                     HostMemoryRequest memory, void *locked, uint64_t bytes);
    [[nodiscard]] Status map(VirtioGpuDevice &device, uint64_t offset);
    [[nodiscard]] Status unmap(VirtioGpuDevice &device);
    [[nodiscard]] Status destroy(VirtioGpuDevice &device);
    void                 abandon();

    bool     created() const;
    bool     mapped() const;
    uint32_t resource() const;
    PageCaching caching() const;
    uint64_t offset() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t map_cache_cached = 1u;
    static constexpr uint32_t map_cache_wc = 3u;
    static constexpr size_t   request_bytes = 64u;
    static constexpr size_t   answer_bytes = 64u;

    [[nodiscard]] Status exchange(VirtioGpuDevice &device, const uint8_t *request, size_t length, uint32_t &map_info);
    [[nodiscard]] Status attach_pieces(VirtioGpuDevice &device, void *locked, uint64_t bytes, uint8_t *request,
                                       uint64_t *frames);

    Platform &platform_;
    uint32_t  resource_;
    uint64_t  offset_;
    bool      mapped_;
    PageCaching caching_;
    friend struct HostVisibleBlobProbe;

    uint8_t   request_[request_bytes];
    uint8_t   answer_[answer_bytes];
};

}

#endif
