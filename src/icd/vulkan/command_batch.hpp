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
    static constexpr size_t command_room = 16u * 1024u;
    static constexpr size_t first_block = 32u * 1024u;

    CommandBatch(BlockSource &memory, Connection &connection);
    ~CommandBatch();
    CommandBatch(const CommandBatch &) = delete;
    CommandBatch &operator=(const CommandBatch &) = delete;

    void                 discard();
    [[nodiscard]] Status end();
    uint32_t             refused() const;

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;
    [[nodiscard]] bool make_room_for_blobs(size_t bytes) override;
    const TransferRoom &room() const override;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    [[nodiscard]] bool make_room();
    [[nodiscard]] bool grow();
    [[nodiscard]] bool flush();
    [[nodiscard]] bool record_spilled(uint32_t command_id, WireWriter &writer);

    BlockSource    &memory_;
    Connection     &connection_;
    FramedBlock     framed_;
    size_t          last_;
    Status          failure_;
    uint32_t        refused_;
    WireArena       decode_arena_;
    ClientBlobArena blobs_;
    TransferRoom    room_;
    SpillBlock      spill_;

    friend struct CommandBatchProbe;
};

}

#endif
