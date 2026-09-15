#include "icd/vulkan/command_batch.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "icd/vulkan/connection.hpp"
#include "proto/submission/command_header.hpp"

namespace av::icd {

namespace {

constexpr size_t header_bytes = CommandHeader::encoded_size;
constexpr size_t doublings = Connection::command_bytes / CommandBatch::first_block;

/* Doubling from the first block lands on the command limit exactly, so a block
 * below the limit never grows past it. */
static_assert(Connection::command_bytes % CommandBatch::first_block == 0u && (doublings & (doublings - 1u)) == 0u);

}  // namespace

CommandBatch::CommandBatch(BlockSource &memory, Connection &connection)
    : memory_(memory), connection_(connection), block_(nullptr), capacity_(0u), used_(0u), last_(0u), failure_(Status::ok),
      decode_arena_(nullptr, 0u), blobs_() {}

CommandBatch::~CommandBatch()
{
    if (block_ != nullptr) memory_.release(block_);
}

void CommandBatch::discard()
{
    used_ = 0u;
    last_ = 0u;
    failure_ = Status::ok;
}

Status CommandBatch::end()
{
    ScopeExit dropped([&] { discard(); });
    if (failure_ != Status::ok) return failure_;
    return connection_.send_batch(block_, used_) == TransportResult::ok ? Status::ok : Status::invalid_state;
}

/* Room for the next command's header and its lent room, or, at the command
 * limit, whatever is left past a header. */
bool CommandBatch::make_room()
{
    if (capacity_ - used_ >= header_bytes + command_room) return true;
    if (capacity_ == Connection::command_bytes) return capacity_ - used_ > header_bytes;
    return grow();
}

/* One doubling is always enough: a block is at least twice the room a
 * command is lent, and what is recorded never exceeds the block. */
bool CommandBatch::grow()
{
    const size_t wanted = capacity_ == 0u ? first_block : capacity_ * 2u;
    auto *const grown = static_cast<uint8_t *>(memory_.allocate(wanted, Pool::paged));
    if (grown == nullptr) return false;
    if (block_ != nullptr) {
        __builtin_memcpy(grown, block_, used_);
        memory_.release(block_);
    }
    block_ = grown;
    capacity_ = wanted;
    return true;
}

WireWriter CommandBatch::request_writer()
{
    if (failure_ != Status::ok) return WireWriter(nullptr, 0u);
    if (!make_room()) {
        failure_ = Status::no_memory;
        return WireWriter(nullptr, 0u);
    }
    const size_t left = capacity_ - used_ - header_bytes;
    return WireWriter(block_ + used_ + header_bytes, left < command_room ? left : command_room);
}

TransportResult CommandBatch::send_async(uint32_t command_id, WireWriter &writer)
{
    if (failure_ != Status::ok) return TransportResult::failed;
    if (writer.status() != Status::ok) {
        failure_ = Status::no_memory;
        return TransportResult::failed;
    }
    const size_t size = header_bytes + writer.offset();
    /* The header has its own room before the body, and the command limit is
     * far below the largest size a header carries. */
    static_cast<void>(CommandHeader(command_id, static_cast<uint32_t>(size), 0u).encode(block_ + used_, header_bytes));
    last_ = used_;
    used_ += size;
    return TransportResult::ok;
}

TransportResult CommandBatch::send_sync(uint32_t, WireWriter &, WireReader &) { return TransportResult::failed; }

WireArena       &CommandBatch::decode_arena() { return decode_arena_; }
ClientBlobArena &CommandBatch::blobs() { return blobs_; }

bool CommandBatch::invariants_hold() const
{
    if (failure_ != Status::ok && failure_ != Status::no_memory) return false;
    if (used_ > capacity_ || capacity_ > Connection::command_bytes || (block_ == nullptr) != (capacity_ == 0u))
        return false;
    if (used_ == 0u) return last_ == 0u;
    CommandHeader last;
    if (last_ >= used_ || last.decode(block_ + last_, used_ - last_) != Status::ok) return false;
    return last_ + last.size() == used_;
}

}  // namespace av::icd
