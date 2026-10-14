/* One virtio-gpu control response the device core sends (virtio 1.2 5.7.6.7):
 * a 24-byte header whose type is the result, or one with its payload -
 * OK_MAP_INFO, OK_CAPSET_INFO, OK_CAPSET. The host fills fence, flags and
 * context from the request. */
#ifndef AV_GPU_RESPONSE_HPP
#define AV_GPU_RESPONSE_HPP

#include "proto/context/capset_info.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GpuResponse {
public:
    enum : uint32_t {
        ok_nodata = 0x1100u,
        ok_capset_info = 0x1102u,
        ok_capset = 0x1103u,
        ok_map_info = 0x1106u,
        err_unspec = 0x1200u,
        err_out_of_memory = 0x1201u,
        err_invalid_resource_id = 0x1203u,
        err_invalid_context_id = 0x1204u,
        err_invalid_parameter = 0x1205u,
    };
    static constexpr uint32_t map_cache_cached = 1u;   /* VIRTIO_GPU_MAP_CACHE_CACHED */
    static constexpr uint32_t map_cache_wc = 3u;       /* VIRTIO_GPU_MAP_CACHE_WC */
    static constexpr size_t   max_length = 24u + CapsetInfo::encoded_size > DisplayModeSet::max_encoded_size
                                               ? 24u + CapsetInfo::encoded_size
                                               : DisplayModeSet::max_encoded_size;

    GpuResponse();

    void           set(uint32_t type);
    void           set_map_info(uint32_t map_info);
    void           set_capset_info(uint32_t capset_id, uint32_t max_version, uint32_t max_size);
    void           set_capset(const CapsetInfo &capset);
    /* OK_AV_DISPLAY_MODES and the set; ERR_UNSPEC for an empty one. */
    void           set_display_modes(const DisplayModeSet &modes);
    uint32_t       type() const;
    const uint8_t *bytes() const;
    size_t         length() const;

private:
    static constexpr size_t header_size = 24u;
    static constexpr size_t map_info_size = 32u;
    static constexpr size_t capset_info_size = 40u;

    uint8_t bytes_[max_length];
    size_t  length_;
    uint32_t type_;
};

}  // namespace av

#endif  // AV_GPU_RESPONSE_HPP
