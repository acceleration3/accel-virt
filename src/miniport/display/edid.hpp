#ifndef AV_EDID_HPP
#define AV_EDID_HPP

#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"

#include <stdint.h>

namespace av {

class DisplayMode;
struct DxgkChildContainerIdRecord;
struct DxgkargUpdateMonitorLinkInfoRecord;
struct DxgkDeviceDescriptorRecord;

class Edid {
public:
    static constexpr uint32_t block_bytes = 128u;

    Edid(Platform &platform, const DisplayMode &preferred);
    void follow(const DisplayMode &preferred);
    Edid(const Edid &) = delete;
    Edid &operator=(const Edid &) = delete;

    NtStatus copy(DxgkDeviceDescriptorRecord &descriptor) const;
    void     describe(DxgkChildContainerIdRecord &container) const;
    void     describe_link(DxgkargUpdateMonitorLinkInfoRecord &link) const;

private:
    static constexpr uint32_t timing_offset = 54u;
    static constexpr uint32_t manufacturer_offset = 8u;
    static constexpr uint32_t product_offset = 10u;
    static constexpr uint32_t descriptor_bytes = 18u;
    static constexpr uint32_t timing_flags_offset = timing_offset + 17u;
    static constexpr uint8_t  timing_stereo_bits = 0x60u;
    static constexpr uint32_t link_stereo = 0x1u;
    static constexpr uint32_t no_more_descriptor_data = 0xC01D0008u;

    void build(const DisplayMode &preferred);
    void write_preferred_timing(const DisplayMode &mode, uint32_t width_mm, uint32_t height_mm);
    void write_display_descriptors();
    void write_checksum();

    Platform &platform_;
    uint8_t   bytes_[block_bytes];
};

}

#endif
