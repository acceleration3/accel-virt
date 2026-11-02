#ifndef AV_SCANOUT_PLANE_REQUEST_HPP
#define AV_SCANOUT_PLANE_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class ScanoutPlaneRequest {
public:
    static constexpr uint32_t type = 0x02F8u;
    static constexpr size_t   encoded_size = 88u;
    static constexpr uint32_t layers = 4u;
    static constexpr uint32_t flag_shown = 0x1u;
    static constexpr uint32_t flag_filtered = 0x2u;
    static constexpr uint32_t flag_ycbcr = 0x4u;
    static constexpr uint32_t flag_bt709 = 0x8u;
    static constexpr uint32_t flag_full_range = 0x10u;
    static constexpr uint32_t most_extent = 16384u;

    ScanoutPlaneRequest();
    ScanoutPlaneRequest(uint32_t head, uint32_t layer, uint32_t flags, uint64_t key, const int32_t *source,
                        const int32_t *destination, uint32_t head_width, uint32_t head_height);

    [[nodiscard]] Status encode(WireWriter &writer) const;
    [[nodiscard]] Status decode(WireReader &reader);
    uint32_t             head() const;
    uint32_t             layer() const;
    bool                 shown() const;
    bool                 filtered() const;
    bool                 ycbcr() const;
    bool                 bt709() const;
    bool                 full_range() const;
    uint64_t             key() const;
    const int32_t       *source() const;
    const int32_t       *destination() const;
    uint32_t             head_width() const;
    uint32_t             head_height() const;

private:
    static constexpr size_t header_size = 24u;

    uint32_t head_;
    uint32_t layer_;
    uint32_t flags_;
    uint64_t key_;
    int32_t  source_[4];
    int32_t  destination_[4];
    uint32_t head_width_;
    uint32_t head_height_;
};

}

#endif
