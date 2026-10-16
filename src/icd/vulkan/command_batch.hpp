/* One command buffer's recorded commands, until vkEndCommandBuffer ships them
 * (research.md §5). It is the transport the generated vkCmd* wrappers are
 * handed for that command buffer: each command is framed as the device reads a
 * batch and appended here instead of sent, and end publishes all of them through
 * the connection at once. A command buffer is externally synchronized, so this
 * takes no lock of its own; the connection takes its send lock to publish.
 *
 * The commands are one contiguous block, because a batch is published as one
 * span; it grows by doubling, copying what was recorded, up to the connection's
 * command limit, and a recording past that goes as several batches: the host
 * records a command buffer's commands in the order they arrive, whichever
 * batch carried them. The generated client encodes a command before its size
 * is known, so each is lent a fixed room, and one that outgrows it spills
 * into memory of the batch's own and is recorded whole - the block growing,
 * or sent to make room - or, longer than any batch, sent alone through the
 * connection (docs/protocol.md, "Long commands"). Memory refused while
 * growing latches one failure, and end answers it rather than ship a batch
 * with a command missing. */
#ifndef AV_ICD_COMMAND_BATCH_HPP
#define AV_ICD_COMMAND_BATCH_HPP

#include "core/support/status.hpp"
#include "guest/transport/framed_block.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/client/transfer_room.hpp"
#include "proto/wire/spill_block.hpp"
#include "proto/wire/wire_arena.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}

namespace av::icd {

class Connection;
struct CommandBatchProbe;

class CommandBatch final : public ClientTransport {
public:
    /* The most one recorded command is lent, and the block a batch starts with. */
    static constexpr size_t command_room = 16u * 1024u;
    static constexpr size_t first_block = 32u * 1024u;

    CommandBatch(BlockSource &memory, Connection &connection);
    ~CommandBatch();
    CommandBatch(const CommandBatch &) = delete;
    CommandBatch &operator=(const CommandBatch &) = delete;

    /* Drops what was recorded and the failure with it; the block is kept for
     * the next recording. */
    void                 discard();
    /* Publishes the recorded commands not yet published, then drops them:
     * no_memory when a command could not be recorded, with nothing more
     * published; invalid_state when the connection refused a batch. */
    [[nodiscard]] Status end();
    /* The command whose failure was the latest latched, when it failed by not
     * fitting the room it was lent; 0 when that failure was anything else. */
    uint32_t             refused() const;

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    /* Always failed: a recorded command is not answered. */
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    /* Nothing to end: no call is ever answered. */
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;
    /* Never: a recording has no arena. */
    [[nodiscard]] bool make_room_for_blobs(size_t bytes) override;
    /* A recorded command's room, and no reply. */
    const TransferRoom &room() const override;

    /* The recording fits a block no larger than the command limit and its last
     * command ends where the recording does; the only failure latched is
     * no_memory. */
    /* The bytes recorded and held, and whether a command failed to record. */
    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    [[nodiscard]] bool make_room();
    [[nodiscard]] bool grow();
    /* Publishes what is recorded so far, at the command limit. */
    [[nodiscard]] bool flush();
    /* A writer that spilled, recorded whole, or sent alone when no batch
     * holds it. */
    [[nodiscard]] bool record_spilled(uint32_t command_id, WireWriter &writer);

    BlockSource    &memory_;
    Connection     &connection_;
    FramedBlock     framed_;
    /* Where the last command recorded starts; 0 with none. */
    size_t          last_;
    Status          failure_;
    uint32_t        refused_;
    WireArena       decode_arena_;
    ClientBlobArena blobs_;
    TransferRoom    room_;
    /* A command longer than its room, while it is written and recorded. */
    SpillBlock      spill_;

    friend struct CommandBatchProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_COMMAND_BATCH_HPP
