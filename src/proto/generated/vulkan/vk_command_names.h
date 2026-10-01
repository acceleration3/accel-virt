#pragma once


#include <stdint.h>

namespace av::vkproto {

// Returns a static C-string name for a wire command id. Returns
// ``"unknown"`` for unknown ids — never null. Cheap (one switch).
[[nodiscard]] const char* cmd_id_to_string(uint32_t cmd_id) noexcept;
// Whether the profile carries `cmd_id`: what a session decodes at all.
[[nodiscard]] bool knows_command(uint32_t cmd_id) noexcept;

}  // namespace av::vkproto
