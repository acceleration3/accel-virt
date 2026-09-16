/* A connection's transport as the generated wrappers see it: every command
 * encoded and sent, and every synchronous reply decoded, by one thread at a
 * time.
 *
 * The generated client asks for a writer, encodes into the room it lends - the
 * inner transport's one staging buffer - and sends. A synchronous call then
 * decodes its reply from the inner transport's one reply slot and ends the call
 * (SyncCallScope). So the lock is taken when the writer is lent and let go when
 * an asynchronous command has been sent, or when a synchronous call ends -
 * otherwise another thread's round trip could replace the reply mid-decode.
 * A command whose payload was dropped before any send is ended by the thread
 * writing it (the OpenGL client), which lets its writer's lock go. Only the
 * thread whose call is open ends it; an end with no call open lets go of
 * nothing. */
#ifndef AV_ICD_LOCKED_TRANSPORT_HPP
#define AV_ICD_LOCKED_TRANSPORT_HPP

#include "proto/client_transport.hpp"

#include <stdint.h>

namespace av::icd {

class ThreadLock;
struct LockedTransportProbe;

class LockedTransport final : public ClientTransport {
public:
    LockedTransport(ClientTransport &inner, ThreadLock &lock);

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

    /* The lock is held exactly while a command is written or a reply read. */
    bool invariants_hold() const;

private:
    enum class Phase : uint8_t { idle, writing, answering };

    void let_go();

    ClientTransport &inner_;
    ThreadLock      &lock_;
    Phase            phase_;
    bool             holds_;

    friend struct LockedTransportProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_LOCKED_TRANSPORT_HPP
