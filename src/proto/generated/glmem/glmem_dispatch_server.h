// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/glmem.toml --out ...

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "proto/generated/glmem/glmem_wire_types.h"
#include "proto/wire/guest_bytes.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av::glmemproto {

// Implemented by the executor; the table below calls into it. Every value
// handed to an on_ method came off the wire: a count is already within the
// registry's maximum and an array holds that many, but what the values mean
// - an object id, a format, a box - is the implementation's to judge. An
// on_ method answers false for a command it refuses, which the device
// treats as one that did not decode.
class ServerDispatch {
public:
    ServerDispatch(const ServerDispatch&) = delete;
    ServerDispatch& operator=(const ServerDispatch&) = delete;

    // `size` bytes at `offset` in the context's blob arena, or null for a range
    // outside it.
    uint8_t* guest_bytes(uint64_t offset, uint64_t size) const;

    virtual bool on_QueryBufferSlicesGL(uint32_t& supported) = 0;
    virtual bool on_PlaceBufferSlice(uint32_t buffer, uint64_t memory, uint64_t offset, uint64_t bytes) = 0;
    virtual bool on_ForgetBufferSlice(uint32_t buffer) = 0;

protected:
    // `guest` is the context's blob arena, which the implementation owns and
    // adopts its first import as.
    explicit ServerDispatch(const ::av::GuestBytes& guest);
    ~ServerDispatch() = default;

private:
    const ::av::GuestBytes& guest_;
};

// Whether the registry has this command at all.
bool knows_command(uint32_t cmd_id);
// Decodes one command and calls the dispatch; false for a frame that does
// not decode, a count past its maximum, or a refusal.
bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, ::av::WireReader& r,
                      ::av::WireWriter& reply, ::av::WireArena& arena);

}  // namespace av::glmemproto
