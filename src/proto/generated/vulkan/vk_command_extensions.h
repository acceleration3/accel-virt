#pragma once


#include <stdint.h>

namespace av::vkproto {

// Whose enabled extensions a command needs one of: the instance's, the
// device's, or nobody's (core commands, and extension commands a physical
// device answers without the extension being enabled).
enum class CommandOwner : uint8_t { open, instance, device };

inline constexpr uint32_t kDeviceExtensionSlots = 52u;
inline constexpr uint32_t kInstanceExtensionSlots = 5u;
inline constexpr uint16_t kNoExtensionSlot = 0xFFFFu;
static_assert(kDeviceExtensionSlots <= 64u && kInstanceExtensionSlots <= 64u, "the enabled mask is 64 bits");

struct CommandExtensions {
    CommandOwner owner;
    uint16_t count;
    const uint16_t* slots;
};

// The extensions of which a command needs at least one enabled.
[[nodiscard]] CommandExtensions command_extensions(uint32_t cmd_id) noexcept;
// The slot an extension name holds among the device or instance
// extensions some command needs; kNoExtensionSlot for any other name.
[[nodiscard]] uint16_t device_extension_slot(const char* name) noexcept;
[[nodiscard]] uint16_t instance_extension_slot(const char* name) noexcept;

}  // namespace av::vkproto
