#pragma once


#include <cstdint>

namespace av::vkproto {

// Returns a static C-string name for a wire command id. Returns
// ``"unknown"`` for unknown ids — never null. Cheap (one switch).
[[nodiscard]] const char* cmd_id_to_string(std::uint32_t cmd_id) noexcept;

}  // namespace av::vkproto
