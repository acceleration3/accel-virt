#include "icd/opengl/buffer_map_transfer.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/pieces.hpp"
#include "icd/opengl/buffer_mapping.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/client/transfer_room.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

BufferMapTransfer::BufferMapTransfer(BufferShadows &shadows) : shadows_(shadows), runs_()
{
    AV_SCOPE("BufferMapTransfer::BufferMapTransfer");
}

void BufferMapTransfer::start(ClientTransport &wire, BufferMapping &mapping)
{
    AV_SCOPE("BufferMapTransfer::start");
    const uint32_t access = mapping.access();
    if ((access & BufferMapping::map_read) != 0u) {
        read(wire, mapping);
        if ((access & BufferMapping::map_write) != 0u)
            shadows_.keep(mapping.buffer(), mapping.offset(), mapping.bytes(), mapping.at());
        return;
    }
    const uint8_t *const known = shadows_.covering(mapping.buffer(), mapping.offset(), mapping.bytes());
    if (known != nullptr) {
        __builtin_memcpy(mapping.at(), known, static_cast<size_t>(mapping.bytes()));
        return;
    }
    const uint32_t keeps_nothing =
        BufferMapping::map_invalidate_range | BufferMapping::map_invalidate_buffer | BufferMapping::map_flush_explicit;
    if ((access & keeps_nothing) != 0u || shadows_.fresh(mapping.buffer())) return;
    read(wire, mapping);
    shadows_.keep(mapping.buffer(), mapping.offset(), mapping.bytes(), mapping.at());
}

void BufferMapTransfer::refresh(ClientTransport &wire, BufferMapping &mapping)
{
    AV_SCOPE("BufferMapTransfer::refresh");
    if (mapping.persistent_writes()) write_back(wire, mapping);
    read(wire, mapping);
}

void BufferMapTransfer::write_back(ClientTransport &wire, BufferMapping &mapping)
{
    AV_SCOPE("BufferMapTransfer::write_back");
    if ((mapping.access() & BufferMapping::map_write) == 0u) return;
    if ((mapping.access() & BufferMapping::map_flush_explicit) != 0u) {
        uint64_t from = 0u, to = 0u;
        mapping.take_marked(from, to);
        send(wire, mapping, from, to);
        shadows_.wrote(mapping.buffer(), mapping.offset() + from, to - from, mapping.at() + from);
        return;
    }
    uint8_t *const known = shadows_.covering(mapping.buffer(), mapping.offset(), mapping.bytes());
    if (known != nullptr) {
        send_changes(wire, mapping, known);
        return;
    }
    send(wire, mapping, 0u, mapping.bytes());
    shadows_.keep(mapping.buffer(), mapping.offset(), mapping.bytes(), mapping.at());
}

void BufferMapTransfer::read(ClientTransport &wire, const BufferMapping &mapping)
{
    AV_SCOPE("BufferMapTransfer::read");
    icd::Pieces pieces(mapping.bytes(), wire.room().inline_read(), wire);
    while (pieces.next())
        glproto::av_glGetNamedBufferSubData(wire, mapping.buffer(),
                                             static_cast<GLintptr>(mapping.offset() + pieces.offset()),
                                             static_cast<GLsizeiptr>(pieces.count()), mapping.at() + pieces.offset());
}

void BufferMapTransfer::send(ClientTransport &wire, const BufferMapping &mapping, uint64_t at, uint64_t end)
{
    AV_SCOPE("BufferMapTransfer::send");
    icd::SplitReport split(wire);
    while (at < end) {
        split.piece();
        const size_t chunk = wire.room().write_piece(wire, end - at);
        glproto::av_glNamedBufferSubData(wire, mapping.buffer(), static_cast<GLintptr>(mapping.offset() + at),
                                          static_cast<GLsizeiptr>(chunk), mapping.at() + at);
        at += chunk;
    }
}

void BufferMapTransfer::send_changes(ClientTransport &wire, const BufferMapping &mapping, uint8_t *known)
{
    AV_SCOPE("BufferMapTransfer::send_changes");
    runs_.clear();
    uint64_t from = next_change(mapping, known, 0u);
    while (from < mapping.bytes()) {
        const uint64_t to = change_end(mapping, known, from);
        if (!runs_.note(from, to - from)) {
            send_runs(wire, mapping, known);
            static_cast<void>(runs_.note(from, to - from));
        }
        from = next_change(mapping, known, to);
    }
    send_runs(wire, mapping, known);
}

void BufferMapTransfer::send_runs(ClientTransport &wire, const BufferMapping &mapping, uint8_t *known)
{
    AV_SCOPE("BufferMapTransfer::send_runs");
    for (uint32_t index = 0u; index < runs_.count(); index++) {
        const icd::WrittenRuns::Run run = runs_.run(index);
        send(wire, mapping, run.offset_, run.offset_ + run.bytes_);
        __builtin_memcpy(known + run.offset_, mapping.at() + run.offset_, static_cast<size_t>(run.bytes_));
    }
    runs_.clear();
}

uint64_t BufferMapTransfer::next_change(const BufferMapping &mapping, const uint8_t *known, uint64_t at) const
{
    AV_SCOPE("BufferMapTransfer::next_change");
    const uint8_t *const copy = mapping.at();
    const uint64_t       length = mapping.bytes();
    for (; at + word <= length; at += word)
        if (__builtin_memcmp(copy + at, known + at, word) != 0) return at;
    const bool tail_differs =
        at < length && __builtin_memcmp(copy + at, known + at, static_cast<size_t>(length - at)) != 0;
    return tail_differs ? at : length;
}

uint64_t BufferMapTransfer::change_end(const BufferMapping &mapping, const uint8_t *known, uint64_t from) const
{
    AV_SCOPE("BufferMapTransfer::change_end");
    const uint8_t *const copy = mapping.at();
    const uint64_t       length = mapping.bytes();
    uint64_t             end = from + word < length ? from + word : length;
    for (; end + word <= length; end += word)
        if (__builtin_memcmp(copy + end, known + end, word) == 0) return end;
    if (end < length && __builtin_memcmp(copy + end, known + end, static_cast<size_t>(length - end)) != 0)
        end = length;
    return end;
}

}
