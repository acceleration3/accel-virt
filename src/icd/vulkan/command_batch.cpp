#include "icd/vulkan/command_batch.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/vulkan/connection.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/command_pieces.hpp"

namespace av::icd {

namespace {

constexpr size_t doublings = Connection::command_bytes / CommandBatch::first_block;

/* Doubling from the first block lands on the command limit exactly, so a block
 * below the limit never grows past it. */
static_assert(Connection::command_bytes % CommandBatch::first_block == 0u && (doublings & (doublings - 1u)) == 0u);

}  // namespace

CommandBatch::CommandBatch(BlockSource &memory, Connection &connection)
    : memory_(memory), connection_(connection), framed_(nullptr, 0u), last_(0u), failure_(Status::ok),
      refused_(0u), decode_arena_(nullptr, 0u), blobs_(), spill_(memory)
      {
          AV_SCOPE("CommandBatch::CommandBatch");
      }

CommandBatch::~CommandBatch()
{
    AV_SCOPE("CommandBatch::~CommandBatch");
    if (framed_.bytes() != nullptr) memory_.release(framed_.bytes());
}

void CommandBatch::discard()
{
    AV_SCOPE("CommandBatch::discard");
    framed_.clear();
    last_ = 0u;
    failure_ = Status::ok;
}

Status CommandBatch::end()
{
    AV_SCOPE("CommandBatch::end");
    ScopeExit dropped([&] { discard(); });
    if (failure_ != Status::ok) return failure_;
    return connection_.send_batch(framed_.bytes(), framed_.used()) == TransportResult::ok ? Status::ok
                                                                                          : Status::invalid_state;
}

/* Room for the next command's header and its lent room: a bigger block below
 * the command limit, and at it, the block emptied into a batch of its own. */
bool CommandBatch::make_room()
{
    AV_SCOPE("CommandBatch::make_room");
    if (framed_.fits(FramedBlock::header_bytes + command_room)) return true;
    if (framed_.capacity() == Connection::command_bytes) return flush();
    return grow();
}

uint32_t CommandBatch::refused() const
{
    AV_SCOPE("CommandBatch::refused");
    return refused_;
}

bool CommandBatch::flush()
{
    AV_SCOPE("CommandBatch::flush");
    if (connection_.send_batch(framed_.bytes(), framed_.used()) != TransportResult::ok) return false;
    framed_.clear();
    last_ = 0u;
    return true;
}

/* One doubling is always enough: a block is at least twice the room a
 * command is lent, and what is recorded never exceeds the block. */
bool CommandBatch::grow()
{
    AV_SCOPE("CommandBatch::grow");
    const size_t wanted = framed_.capacity() == 0u ? first_block : framed_.capacity() * 2u;
    auto *const grown = static_cast<uint8_t *>(memory_.allocate(wanted, Pool::paged));
    if (grown == nullptr) return false;
    uint8_t *const old = framed_.bytes();
    if (old != nullptr) {
        __builtin_memcpy(grown, old, framed_.used());
        memory_.release(old);
    }
    framed_.adopt(grown, wanted);
    return true;
}

WireWriter CommandBatch::request_writer()
{
    AV_SCOPE("CommandBatch::request_writer");
    if (failure_ != Status::ok) return WireWriter(nullptr, 0u);
    if (!make_room()) {
        failure_ = Status::no_memory;
        refused_ = 0u;
        return WireWriter(nullptr, 0u);
    }
    return framed_.lend(command_room, spill_);
}

TransportResult CommandBatch::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("CommandBatch::send_async");
    if (failure_ != Status::ok) return TransportResult::failed;
    const size_t before = framed_.used();
    if (writer.spilled()) {
        if (record_pieces(command_id, writer)) return TransportResult::ok;
        failure_ = Status::no_memory;
        refused_ = command_id;
        return TransportResult::failed;
    }
    /* The writer was lent at most the block's room, so only one that failed
     * is not framed. */
    if (!framed_.frame(command_id, writer, 0u)) {
        failure_ = Status::no_memory;
        refused_ = command_id;
        return TransportResult::failed;
    }
    last_ = before;
    return TransportResult::ok;
}

/* Each piece where the last command recorded would be: a full block grows,
 * and one at the command limit goes as a batch of its own first. */
bool CommandBatch::record_pieces(uint32_t command_id, const WireWriter &writer)
{
    AV_SCOPE("CommandBatch::record_pieces");
    ScopeExit given_back([this] { spill_.release(); });
    if (writer.status() != Status::ok) return false;
    CommandPieces pieces(command_id, 0u, writer.bytes(), writer.offset());
    while (!pieces.done()) {
        const size_t before = framed_.used();
        if (framed_.frame_piece(pieces)) {
            last_ = before;
            continue;
        }
        if (framed_.capacity() == Connection::command_bytes ? !flush() : !grow()) return false;
    }
    return true;
}

TransportResult CommandBatch::send_sync(uint32_t, WireWriter &, WireReader &)
{
    AV_SCOPE("CommandBatch::send_sync");
    return TransportResult::failed;
}

void CommandBatch::end_call()
{
    AV_SCOPE("CommandBatch::end_call");
}

WireArena       &CommandBatch::decode_arena()
{
    AV_SCOPE("CommandBatch::decode_arena");
    return decode_arena_;
}
ClientBlobArena &CommandBatch::blobs()
{
    AV_SCOPE("CommandBatch::blobs");
    return blobs_;
}

void CommandBatch::report(StateWriter &writer) const
{
    AV_SCOPE("CommandBatch::report");
    writer.number("recorded", framed_.used());
    writer.number("capacity", framed_.capacity());
    writer.flag("failed", failure_ != Status::ok);
}

bool CommandBatch::invariants_hold() const
{
    AV_SCOPE("CommandBatch::invariants_hold");
    if (failure_ != Status::ok && failure_ != Status::no_memory) return false;
    if (!framed_.invariants_hold() || framed_.capacity() > Connection::command_bytes) return false;
    const size_t used = framed_.used();
    if (used == 0u) return last_ == 0u;
    CommandHeader last;
    if (last_ >= used || last.decode(framed_.bytes() + last_, used - last_) != Status::ok) return false;
    return last_ + last.size() == used;
}

}  // namespace av::icd
