#include "core/display/edid.hpp"

#include "core/ddi/dxgk_child_container_id_record.hpp"
#include "core/ddi/dxgk_device_descriptor_record.hpp"
#include "core/ddi/dxgkarg_update_monitor_link_info_record.hpp"
#include "core/display/display_mode.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

/* Header, "RVK" product 1 of 2026, version 1.3, digital input, then (after the
 * size in centimetres) gamma 2.2, RGB colour with sRGB primaries and a
 * preferred timing, the sRGB chromaticity, 640x480, 800x600 and 1024x768 at
 * 60 Hz established, and no standard timings. */
constexpr uint8_t identity[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x4A, 0xCB, 0x01, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x2E, 0x01, 0x03, 0x80};
constexpr uint8_t colour[] = {0x78, 0x0E, 0xEE, 0x91, 0xA3, 0x54, 0x4C, 0x99, 0x26, 0x0F, 0x50, 0x54,
                              0x21, 0x08, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                              0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
/* Vertical 50-75 Hz, horizontal 30-90 kHz, pixel clock to 150 MHz. */
constexpr uint8_t range_limits[] = {0x00, 0x00, 0x00, 0xFD, 0x00, 50u, 75u, 30u, 90u, 15u,
                                    0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
constexpr uint8_t monitor_name[] = {0x00, 0x00, 0x00, 0xFC, 0x00, 'a', 'c', 'c', 'e', 'l', '-', 'v',
                                    'i', 'r', 't', 0x0A, 0x20, 0x20};
constexpr uint8_t dummy[] = {0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

constexpr uint32_t millimetres_per_inch_tenths = 254u;
constexpr uint32_t dots_per_inch_tenths = 960u;
constexpr uint32_t pixel_clock_hz_per_unit = 10000u;
constexpr uint8_t  horizontal_front_porch = 48u;
constexpr uint8_t  horizontal_sync = 32u;
constexpr uint8_t  vertical_front_porch = 3u;
constexpr uint8_t  vertical_sync = 6u;
constexpr uint8_t  digital_separate_sync_positive_hsync = 0x1Au;

}  // namespace

Edid::Edid(Platform &platform, const DisplayMode &preferred) : platform_(platform), bytes_{}
{
    AV_SCOPE("Edid::Edid");
    build(preferred);
}

void Edid::follow(const DisplayMode &preferred)
{
    AV_SCOPE("Edid::follow");
    platform_.require_irql_at_most(Irql::passive, "Edid::follow");
    build(preferred);
}

void Edid::build(const DisplayMode &preferred)
{
    AV_SCOPE("Edid::build");
    const uint32_t width_mm = preferred.width() * millimetres_per_inch_tenths / dots_per_inch_tenths;
    const uint32_t height_mm = preferred.height() * millimetres_per_inch_tenths / dots_per_inch_tenths;
    WireWriter writer(bytes_, timing_offset);
    writer.copy_bytes(identity, sizeof identity);
    writer.write_u8(static_cast<uint8_t>(width_mm / 10u));
    writer.write_u8(static_cast<uint8_t>(height_mm / 10u));
    writer.copy_bytes(colour, sizeof colour);
    write_preferred_timing(preferred, width_mm, height_mm);
    write_display_descriptors();
    write_checksum();
}

NtStatus Edid::copy(DxgkDeviceDescriptorRecord &descriptor) const
{
    AV_SCOPE("Edid::copy");
    platform_.require_irql_at_most(Irql::passive, "Edid::copy");
    if (descriptor.DescriptorOffset >= block_bytes) return NtStatus(no_more_descriptor_data);
    if (descriptor.DescriptorBuffer == nullptr) return NtStatus(Status::invalid_parameter);
    const uint32_t remaining = block_bytes - descriptor.DescriptorOffset;
    const uint32_t count = descriptor.DescriptorLength < remaining ? descriptor.DescriptorLength : remaining;
    WireWriter(descriptor.DescriptorBuffer, count).copy_bytes(&bytes_[descriptor.DescriptorOffset], count);
    descriptor.DescriptorLength = count;
    return NtStatus(Status::ok);
}

/* Every field is 8 bits plus a high nibble packed with its neighbour's. */
void Edid::describe(DxgkChildContainerIdRecord &container) const
{
    AV_SCOPE("Edid::describe");
    platform_.require_irql_at_most(Irql::passive, "Edid::describe");
    container.EldInfoPortId = 0u;
    container.EldInfoManufacturerName =
        static_cast<uint16_t>(bytes_[manufacturer_offset] | bytes_[manufacturer_offset + 1u] << 8u);
    container.EldInfoProductCode = static_cast<uint16_t>(bytes_[product_offset] | bytes_[product_offset + 1u] << 8u);
}

void Edid::write_preferred_timing(const DisplayMode &mode, uint32_t width_mm, uint32_t height_mm)
{
    AV_SCOPE("Edid::write_preferred_timing");
    const auto low_byte = [](uint32_t value) { return static_cast<uint8_t>(value & 0xFFu); };
    const auto high_nibbles = [](uint32_t first, uint32_t second) {
        return static_cast<uint8_t>(((first >> 8u) & 0x0Fu) << 4u | ((second >> 8u) & 0x0Fu));
    };
    const uint32_t horizontal_blank = mode.total_width() - mode.width();
    const uint32_t vertical_blank = mode.total_height() - mode.height();
    WireWriter writer(&bytes_[timing_offset], descriptor_bytes);
    writer.write_u16(static_cast<uint16_t>(mode.pixel_rate() / pixel_clock_hz_per_unit));
    writer.write_u8(low_byte(mode.width()));
    writer.write_u8(low_byte(horizontal_blank));
    writer.write_u8(high_nibbles(mode.width(), horizontal_blank));
    writer.write_u8(low_byte(mode.height()));
    writer.write_u8(low_byte(vertical_blank));
    writer.write_u8(high_nibbles(mode.height(), vertical_blank));
    writer.write_u8(horizontal_front_porch);
    writer.write_u8(horizontal_sync);
    writer.write_u8(static_cast<uint8_t>(vertical_front_porch << 4u | vertical_sync));
    writer.write_u8(0u);
    writer.write_u8(low_byte(width_mm));
    writer.write_u8(low_byte(height_mm));
    writer.write_u8(high_nibbles(width_mm, height_mm));
    writer.write_zeros(2u);
    writer.write_u8(digital_separate_sync_positive_hsync);
}

void Edid::write_display_descriptors()
{
    AV_SCOPE("Edid::write_display_descriptors");
    WireWriter writer(&bytes_[timing_offset + descriptor_bytes], 3u * descriptor_bytes + 1u);
    writer.copy_bytes(range_limits, sizeof range_limits);
    writer.copy_bytes(monitor_name, sizeof monitor_name);
    writer.copy_bytes(dummy, sizeof dummy);
    writer.write_u8(0u);   /* no extension blocks */
}

void Edid::write_checksum()
{
    AV_SCOPE("Edid::write_checksum");
    uint8_t sum = 0u;
    for (uint32_t index = 0u; index + 1u < block_bytes; index++) sum = static_cast<uint8_t>(sum + bytes_[index]);
    bytes_[block_bytes - 1u] = static_cast<uint8_t>(0u - sum);
}

void Edid::describe_link(DxgkargUpdateMonitorLinkInfoRecord &link) const
{
    AV_SCOPE("Edid::describe_link");
    platform_.require_irql_at_most(Irql::passive, "Edid::describe_link");
    const uint32_t stereo = (bytes_[timing_flags_offset] & timing_stereo_bits) != 0u;
    link.MonitorLinkInfoCapabilities = stereo * link_stereo;
    link.MonitorLinkInfoDitheringSupport = 0u;
}

}  // namespace av
