// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opengl --xml gl.xml --wgl wgl.xml
//                  --profile tools/codegen/profiles/opengl.toml --out ...

#pragma once

#include <cstddef>
#include <cstdint>

#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_writer.hpp"
#include "proto/wire/wire_reader.hpp"
#include "gl_real_dispatch.h"

namespace av::glproto {

// Reads one command and calls the host's entry point for it.
//
// `arena` is where everything a decoded command points at lives,
// for the length of the call; it is the caller's block, because
// it has to outlive the decode that filled it.
[[nodiscard]] bool dispatch_command(const RealGl& gl, std::uint32_t cmd_id,
                                    ::av::WireReader& reader,
                                    ::av::WireWriter& reply,
                                    ::av::WireArena& arena) noexcept;

}  // namespace av::glproto
