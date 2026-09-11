/* The EDID 1.3 base block of the adapter's synthetic monitor: a digital sRGB
 * panel at 96 dpi whose preferred timing is the preferred mode. dxgkrnl reads
 * it through DxgkDdiQueryDeviceDescriptor. */
#ifndef AV_EDID_HPP
#define AV_EDID_HPP

#include "nt_status.hpp"
#include "platform.hpp"

#include <stdint.h>

namespace av {

class DisplayMode;
struct DxgkDeviceDescriptorRecord;

class Edid {
public:
    static constexpr uint32_t block_bytes = 128u;

    Edid(Platform &platform, const DisplayMode &preferred);
    Edid(const Edid &) = delete;
    Edid &operator=(const Edid &) = delete;

    /* Copies from DescriptorOffset up to DescriptorLength bytes, no further than
     * the end of the block, and answers the count copied in DescriptorLength. */
    NtStatus copy(DxgkDeviceDescriptorRecord &descriptor) const;

private:
    static constexpr uint32_t timing_offset = 54u;
    static constexpr uint32_t descriptor_bytes = 18u;
    /* STATUS_MONITOR_NO_MORE_DESCRIPTOR_DATA */
    static constexpr uint32_t no_more_descriptor_data = 0xC01D0008u;

    void write_preferred_timing(const DisplayMode &mode, uint32_t width_mm, uint32_t height_mm);
    void write_display_descriptors();
    void write_checksum();

    Platform &platform_;
    uint8_t   bytes_[block_bytes];
};

}  // namespace av

#endif  // AV_EDID_HPP
