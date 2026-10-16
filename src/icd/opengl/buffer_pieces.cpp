#include "icd/opengl/buffer_pieces.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/pieces.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/client/transfer_room.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

BufferPieces::BufferPieces(ClientTransport &wire) : wire_(wire), commands_(0u)
{
    AV_SCOPE("BufferPieces::BufferPieces");
}

bool BufferPieces::cuts_write(intptr_t size, const void *data)
{
    AV_SCOPE("BufferPieces::cuts_write");
    if (data == nullptr || size <= static_cast<intptr_t>(wire_.room().inline_write()) || size > static_cast<intptr_t>(max_bytes))
        return false;
    return !lends_whole(static_cast<size_t>(size));
}

bool BufferPieces::cuts_read(intptr_t size, const void *data) const
{
    AV_SCOPE("BufferPieces::cuts_read");
    return data != nullptr && size > static_cast<intptr_t>(wire_.room().inline_read()) && size <= static_cast<intptr_t>(max_bytes);
}

bool BufferPieces::lends_whole(size_t bytes)
{
    AV_SCOPE("BufferPieces::lends_whole");
    ClientBlobArena &blobs = wire_.blobs();
    return blobs.ready() && blobs.room() >= bytes;
}

void BufferPieces::write(uint32_t target, intptr_t offset, size_t bytes, const uint8_t *from)
{
    AV_SCOPE("BufferPieces::write");
    icd::Pieces pieces(bytes, wire_.room().inline_write());
    while (pieces.next()) {
        glproto::av_glBufferSubData(wire_, target, offset + static_cast<intptr_t>(pieces.offset()),
                                    static_cast<intptr_t>(pieces.count()), from + pieces.offset());
        commands_++;
    }
}

void BufferPieces::write_named(uint32_t buffer, intptr_t offset, size_t bytes, const uint8_t *from)
{
    AV_SCOPE("BufferPieces::write_named");
    icd::Pieces pieces(bytes, wire_.room().inline_write());
    while (pieces.next()) {
        glproto::av_glNamedBufferSubData(wire_, buffer, offset + static_cast<intptr_t>(pieces.offset()),
                                         static_cast<intptr_t>(pieces.count()), from + pieces.offset());
        commands_++;
    }
}

void BufferPieces::write_named_ext(uint32_t buffer, intptr_t offset, size_t bytes, const uint8_t *from)
{
    AV_SCOPE("BufferPieces::write_named_ext");
    icd::Pieces pieces(bytes, wire_.room().inline_write());
    while (pieces.next()) {
        glproto::av_glNamedBufferSubDataEXT(wire_, buffer, offset + static_cast<intptr_t>(pieces.offset()),
                                            static_cast<intptr_t>(pieces.count()), from + pieces.offset());
        commands_++;
    }
}

void BufferPieces::read(uint32_t target, intptr_t offset, size_t bytes, uint8_t *into)
{
    AV_SCOPE("BufferPieces::read");
    icd::Pieces pieces(bytes, wire_.room().inline_read());
    while (pieces.next()) {
        glproto::av_glGetBufferSubData(wire_, target, offset + static_cast<intptr_t>(pieces.offset()),
                                       static_cast<intptr_t>(pieces.count()), into + pieces.offset());
        commands_++;
    }
}

void BufferPieces::read_named(uint32_t buffer, intptr_t offset, size_t bytes, uint8_t *into)
{
    AV_SCOPE("BufferPieces::read_named");
    icd::Pieces pieces(bytes, wire_.room().inline_read());
    while (pieces.next()) {
        glproto::av_glGetNamedBufferSubData(wire_, buffer, offset + static_cast<intptr_t>(pieces.offset()),
                                            static_cast<intptr_t>(pieces.count()), into + pieces.offset());
        commands_++;
    }
}

void BufferPieces::read_named_ext(uint32_t buffer, intptr_t offset, size_t bytes, uint8_t *into)
{
    AV_SCOPE("BufferPieces::read_named_ext");
    icd::Pieces pieces(bytes, wire_.room().inline_read());
    while (pieces.next()) {
        glproto::av_glGetNamedBufferSubDataEXT(wire_, buffer, offset + static_cast<intptr_t>(pieces.offset()),
                                               static_cast<intptr_t>(pieces.count()), into + pieces.offset());
        commands_++;
    }
}

uint32_t BufferPieces::commands() const
{
    AV_SCOPE("BufferPieces::commands");
    return commands_;
}

}  // namespace av::glicd
