/* The host GPU's name on its way to Windows: whatever bytes a Vulkan driver
 * put in deviceName become the capset's name (AdapterName, on the device) and
 * that name becomes the UTF-16 the guest's DeviceDesc and registry values hold
 * (Utf16Text, in the miniport). The name must be a prefix of what the driver
 * said, fit the capset's field, and never end inside a character; the UTF-16
 * must be terminated where bytes() says and hold no unpaired surrogate. Any
 * breach traps. */
#include "miniport/adapter/utf16_text.hpp"
#include "device/gpu/adapter_name.hpp"
#include "proto/context/gpu_identity.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t   max_input = 256u;
constexpr std::uint16_t high_first = 0xD800u;
constexpr std::uint16_t low_first = 0xDC00u;
constexpr std::uint16_t low_last = 0xDFFFu;

bool well_formed(const av::Utf16Text &text)
{
    const std::size_t units = text.bytes() / sizeof(std::uint16_t);
    if (units == 0u || units > av::Utf16Text::capacity || text.units()[units - 1u] != 0u) return false;
    for (std::size_t index = 0u; index + 1u < units; index++) {
        const std::uint16_t unit = text.units()[index];
        if (unit == 0u) return false;
        const bool high = unit >= high_first && unit < low_first;
        const bool low = unit >= low_first && unit <= low_last;
        if (low) return false;
        if (!high) continue;
        const std::uint16_t next = text.units()[index + 1u];
        if (next < low_first || next > low_last) return false;
        index++;
    }
    return true;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    char name[max_input + 1u] = {};
    std::memcpy(name, data, size < max_input ? size : max_input);
    const av::AdapterName adapter(name);
    const std::size_t     kept = std::strlen(adapter.text());
    if (kept >= av::GpuIdentity::name_bytes || std::memcmp(adapter.text(), name, kept) != 0) __builtin_trap();
    /* A cut lands before a character, never in one; a name of nothing but
     * continuation bytes is cut to nothing. */
    if (kept != 0u && (static_cast<unsigned char>(name[kept]) & 0xC0u) == 0x80u) __builtin_trap();
    if (!well_formed(av::Utf16Text(adapter.text())) || !well_formed(av::Utf16Text(name))) __builtin_trap();
    return 0;
}
