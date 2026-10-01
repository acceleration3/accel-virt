// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.opengl --xml gl.xml --wgl wgl.xml
//                  --profile tools/codegen/profiles/opengl.toml --out ...

#pragma once

#include <stdint.h>

namespace av::glproto {

const char* cmd_id_to_string(uint32_t cmd_id) noexcept;

// Whether the profile carries `cmd_id`: what a session decodes at all.
bool knows_command(uint32_t cmd_id) noexcept;

}  // namespace av::glproto
