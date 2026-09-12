/* The EDID 1.3 base block of the adapter's synthetic monitor: a digital sRGB
 * panel at 96 dpi whose preferred timing is the preferred mode. dxgkrnl reads
 * it through DxgkDdiQueryDeviceDescriptor. */
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
    /* The block again, for the mode the device's display info made preferred. */
    void follow(const DisplayMode &preferred);
    Edid(const Edid &) = delete;
    Edid &operator=(const Edid &) = delete;

    /* Copies from DescriptorOffset up to DescriptorLength bytes, no further than
     * the end of the block, and answers the count copied in DescriptorLength. */
    NtStatus copy(DxgkDeviceDescriptorRecord &descriptor) const;
    /* The ELD fields: the manufacturer id in the EDID's byte order, the product
     * code, port 0. The container GUID is left as it is. */
    void     describe(DxgkChildContainerIdRecord &container) const;
    /* DXGK_MONITORLINKINFO capabilities and dithering support: stereo when the
     * preferred timing is stereo; no wide or high colour without a colour
     * extension block. The usage hints are left as the OS gave them. */
    void     describe_link(DxgkargUpdateMonitorLinkInfoRecord &link) const;

private:
    static constexpr uint32_t timing_offset = 54u;
    static constexpr uint32_t manufacturer_offset = 8u;
    static constexpr uint32_t product_offset = 10u;
    static constexpr uint32_t descriptor_bytes = 18u;
    static constexpr uint32_t timing_flags_offset = timing_offset + 17u;
    static constexpr uint8_t  timing_stereo_bits = 0x60u;
    static constexpr uint32_t link_stereo = 0x1u;   /* DXGK_MONITORLINKINFO_CAPABILITIES */
    /* STATUS_MONITOR_NO_MORE_DESCRIPTOR_DATA */
    static constexpr uint32_t no_more_descriptor_data = 0xC01D0008u;

    void build(const DisplayMode &preferred);
    void write_preferred_timing(const DisplayMode &mode, uint32_t width_mm, uint32_t height_mm);
    void write_display_descriptors();
    void write_checksum();

    Platform &platform_;
    uint8_t   bytes_[block_bytes];
};

}  // namespace av

#endif  // AV_EDID_HPP
