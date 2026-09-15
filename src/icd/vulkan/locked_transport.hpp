/* A connection's transport as the generated wrappers see it: every command
 * encoded and sent by one thread at a time.
 *
 * The generated client asks for a writer, encodes into the room it lends - the
 * inner transport's one staging buffer - and sends. So the lock is taken when the
 * writer is lent and let go when the command has been sent; every generated
 * rvk_vk* function calls request_writer once and then exactly one send_*.
 *
 * What it cannot cover: a synchronous call's reply is decoded after send_sync
 * returns, from the inner transport's reply slot, with the lock already
 * released, so another thread's round trip on the same connection can replace it
 * mid-decode. Closing that needs ClientTransport to learn when a call ends,
 * which is a change to the 063 emitters (integration point). */
#ifndef AV_ICD_LOCKED_TRANSPORT_HPP
#define AV_ICD_LOCKED_TRANSPORT_HPP

#include "proto/client_transport.hpp"

#include <stdint.h>

namespace av::icd {

class ThreadLock;

class LockedTransport final : public ClientTransport {
public:
    LockedTransport(ClientTransport &inner, ThreadLock &lock);

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

private:
    ClientTransport &inner_;
    ThreadLock      &lock_;
};

}  // namespace av::icd

#endif  // AV_ICD_LOCKED_TRANSPORT_HPP
