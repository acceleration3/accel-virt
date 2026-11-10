#ifndef AV_ICD_GL_HELPERS_H
#define AV_ICD_GL_HELPERS_H

#include "gl_types.h"

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::glproto {

size_t   pname_count(ClientTransport &transport, uint32_t pname) noexcept;
size_t   uniform_value_count(ClientTransport &transport, GLuint program, GLint location) noexcept;
size_t   uniform_block_value_count(ClientTransport &transport, GLuint program, GLuint block, GLenum pname) noexcept;
size_t   map_value_count(ClientTransport &transport, GLenum target, GLenum query) noexcept;
size_t   map_point_values(GLenum target) noexcept;
size_t   pixel_map_size(ClientTransport &transport, GLenum map) noexcept;
size_t   stipple_bytes(ClientTransport &transport, bool packing) noexcept;
size_t   bitmap_bytes(ClientTransport &transport, GLsizei width, GLsizei height) noexcept;
size_t   program_string_bytes(ClientTransport &transport) noexcept;
size_t   named_program_string_bytes(ClientTransport &transport, GLuint program, GLenum target) noexcept;
uint64_t mint_sync_token() noexcept;

}

#endif
