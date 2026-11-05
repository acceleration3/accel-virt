// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/glmem.toml --out ...

#pragma once

#include <stdint.h>

#include "proto/client/client_transport.hpp"
#include "proto/generated/glmem/glmem_wire_types.h"

namespace av::glmemproto {

// One wrapper per command: false when it could not be encoded or sent, or -
// for a command with a reply - when the answer did not decode.

// Whether an OpenGL buffer can be placed in a slice of a block of slice memory (PlaceBufferSlice): 1 when the
// host's context imports memory objects, places a buffer in one by name at an offset, and marks each of its batches
// with a fence the GPU signals, 0 when it cannot - the guest then keeps its copies of what it maps, as before
// (docs/mapped-buffers-plan.md).
[[nodiscard]] bool av_glmem_QueryBufferSlicesGL(::av::ClientTransport& t, uint32_t* supported);
// The buffer `buffer` of the context, which has no storage yet, from now on is `bytes` at `offset` of the memory object
// `memory` - a block of the context's slice memory, named by the handle IMPORT_BLOB answered (docs/host-memory.md) - which the
// guest writes and the host GPU reads or writes in place. Answers 1 when the host placed it, and 0 when it did not: a `memory`
// the context did not import, an `offset` and `bytes` that are not inside it, a buffer that already has immutable storage, or a
// host GL that refused the storage. A refusal records no GL error and counts nothing; the guest then specifies the buffer's
// storage the ordinary way. The host keeps an imported block until its GPU work has run and every buffer placed in it was
// forgotten or deleted.
[[nodiscard]] bool av_glmem_PlaceBufferSlice(::av::ClientTransport& t, uint32_t buffer, uint64_t memory, uint64_t offset, uint64_t bytes, uint32_t* placed);
// The buffer was placed in a slice (PlaceBufferSlice) and is deleted by the commands before this one: the host stops
// counting it among the buffers that keep the block it was placed in.
[[nodiscard]] bool av_glmem_ForgetBufferSlice(::av::ClientTransport& t, uint32_t buffer);

}  // namespace av::glmemproto
