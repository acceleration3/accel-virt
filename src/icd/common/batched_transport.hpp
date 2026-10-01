/* The commands a transport would send a batch each, sent as one batch: each
 * command is framed into a block of the caller's, and the block goes out when
 * the room left could not take another command, when a command wants an
 * answer (it is the batch's last), and when the owner flushes. A command a
 * call at a time was a batch, a fence and a publish of the tail each; an
 * OpenGL application's thousands of calls a frame waited on the host taking
 * them one at a time. */
#ifndef AV_ICD_BATCHED_TRANSPORT_HPP
#define AV_ICD_BATCHED_TRANSPORT_HPP

#include "icd/common/framed_block.hpp"
#include "proto/client_transport.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class KmtTransport;
}

namespace av::icd {

class BatchedTransport final : public ClientTransport {
public:
    /* `block` is the caller's, `capacity` bytes, and holds more than one
     * command of `largest_command` bytes. */
    BatchedTransport(KmtTransport &inner, uint8_t *block, size_t capacity, size_t largest_command);
    BatchedTransport(const BatchedTransport &) = delete;
    BatchedTransport &operator=(const BatchedTransport &) = delete;

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

    /* What is held goes to the host now: before another context may rely on
     * it, and at glFlush. */
    [[nodiscard]] TransportResult flush();
    size_t                        pending() const;

private:
    TransportResult append(uint32_t command_id, WireWriter &writer, uint8_t flags);

    KmtTransport &inner_;
    FramedBlock   framed_;
    size_t        largest_;
    bool          failed_;
};

}  // namespace av::icd

#endif  // AV_ICD_BATCHED_TRANSPORT_HPP
