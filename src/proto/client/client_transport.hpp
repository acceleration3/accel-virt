/* What one guest context's generated client dispatch is handed.
 *
 * The generated client for all three APIs takes a `ClientTransport&` and calls
 * only what is here: it asks for a writer over the room one command may fill,
 * encodes into it, and sends - discarding the answer, or waiting for one. Two
 * things implement this: `KmtTransport` in a guest over D3DKMT, and the L4
 * loopback rig's transport in a test, so it is virtual for the reason
 * `Platform` is (feature 043; contracts/client_transport.md).
 *
 * The writer and reader are fixed-capacity views the transport owns the storage
 * for; `wire_*` is unchanged. `send_*` refuses a writer that overflowed rather
 * than send a short command - the overflow check the generated Vulkan and
 * OpenCL clients never had and OpenGL always did, in one place. */
#ifndef AV_CLIENT_TRANSPORT_HPP
#define AV_CLIENT_TRANSPORT_HPP

#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClientBlobArena;
class TransferRoom;

enum class TransportResult : uint32_t {
    ok = 0u,
    /* The writer overflowed its slot, the context has faulted, or the answer
     * did not fit the reply slot: a failed call, and the client returns the
     * API's own error. */
    failed
};

class ClientTransport {
public:
    ClientTransport(const ClientTransport &) = delete;
    ClientTransport &operator=(const ClientTransport &) = delete;

    /* A writer over the room the next command may fill. The client encodes into
     * it and does not check per field; an overflow latches and is caught by
     * send_*. One at a time: a second before the first is sent is a misuse. */
    virtual WireWriter request_writer() = 0;
    /* Publish the encoded command; no answer. failed for a writer that
     * overflowed, which faults the context rather than sending a short frame. */
    [[nodiscard]] virtual TransportResult send_async(uint32_t command_id, WireWriter &writer) = 0;
    /* Publish and wait. `reply` is pointed at the answer on ok, and is valid
     * only until end_call. failed for an overflow, a faulted context, or an
     * answer too large for the slot. */
    [[nodiscard]] virtual TransportResult send_sync(uint32_t command_id, WireWriter &writer,
                                                    WireReader &reply) = 0;
    /* The synchronous call's reply has been decoded, whatever send_sync
     * answered; called once after each send_sync (SyncCallScope). A transport
     * shared between threads holds it for the caller until here. */
    virtual void end_call() = 0;
    /* Where a reply decodes. On the guest it refuses every allocation: a
     * decoded reply fills the application's own structures, and anything a
     * decode would allocate is scratch that dies with the call. */
    virtual WireArena &decode_arena() = 0;
    /* The arena a large array is lent room in, to cross as a blob rather than
     * inline (OpenGL). `ready()` false means fall back to inline. */
    virtual ClientBlobArena &blobs() = 0;
    /* Room for a `bytes` lease in blobs(), made by having the host run the
     * oldest batches whose leases are still live - what is held sent first.
     * False with no arena, when even the host having run everything leaves
     * too little, or when the context faulted. */
    [[nodiscard]] virtual bool make_room_for_blobs(size_t bytes) = 0;
    /* What one piece of a bulk transfer carries on this transport. */
    virtual const TransferRoom &room() const = 0;

protected:
    ClientTransport() = default;
    /* Not virtual: nothing is destroyed through this pointer, and a deleting
     * destructor would need an operator delete the kernel build has not
     * (CLAUDE.md, Adaptations). */
    ~ClientTransport() = default;
};

}  // namespace av

#endif  // AV_CLIENT_TRANSPORT_HPP
