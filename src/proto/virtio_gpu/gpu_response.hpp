/* One virtio-gpu control response the device core sends (virtio 1.2 5.7.6.7):
 * a 24-byte header whose type is the result, or OK_MAP_INFO with its 8 bytes.
 * The host fills fence, flags and context from the request. */
#ifndef AV_GPU_RESPONSE_HPP
#define AV_GPU_RESPONSE_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class GpuResponse {
public:
    enum : uint32_t {
        ok_nodata = 0x1100u,
        ok_map_info = 0x1106u,
        err_unspec = 0x1200u,
        err_out_of_memory = 0x1201u,
        err_invalid_resource_id = 0x1203u,
        err_invalid_parameter = 0x1205u,
    };
    static constexpr uint32_t map_cache_cached = 1u;   /* VIRTIO_GPU_MAP_CACHE_CACHED */

    GpuResponse();

    void           set(uint32_t type);
    void           set_map_info(uint32_t map_info);
    uint32_t       type() const;
    const uint8_t *bytes() const;
    size_t         length() const;

private:
    static constexpr size_t header_size = 24u;
    static constexpr size_t map_info_size = 32u;

    uint8_t bytes_[map_info_size];
    size_t  length_;
    uint32_t type_;
};

}  // namespace av

#endif  // AV_GPU_RESPONSE_HPP
