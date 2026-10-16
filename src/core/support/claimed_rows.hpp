/* Rows appended from any thread, read without a lock from any thread: what a
 * driver remembers once and looks up from inside its own locks - a log line
 * already written, an answer already given. No row is ever changed or taken
 * back.
 *
 * A writer claims the next row with one atomic step, writes it and then marks
 * it ready; a reader sees a row only once it is ready, so never a half-written
 * one. The rows live in chunks that double in size as they are added and never
 * move, because a reader walking them holds no lock a growth could wait for:
 * a realloc would pull the rows out from under it. Two writers that both find
 * a chunk missing each make one; the first to publish it wins and the other
 * gives its back.
 *
 * A claimed row whose chunk could not be made is never ready: the append
 * answers no_memory and readers pass over it. Row is trivially copyable.
 * Appending allocates, so it is legal up to APC; reading is legal anywhere. */
#ifndef AV_CLAIMED_ROWS_HPP
#define AV_CLAIMED_ROWS_HPP

#include "core/platform/block_source.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct ClaimedRowsProbe;

template <typename Row>
class ClaimedRows {
    static_assert(__is_trivially_copyable(Row), "ClaimedRows copies its rows bytewise");

    struct Cell {
        Row  row_;
        bool ready_;
    };

public:
    /* The first chunk's rows; chunk n holds first_rows << n. */
    static constexpr uint32_t first_rows = 16u;
    /* Chunks a table may have: each one's bytes counted by a size_t on the
     * 32-bit guest builds as on the host. */
    static constexpr uint32_t most_chunks =
        (~size_t{0} / 4u / sizeof(Cell)) / first_rows >= (size_t{1} << 26u)
            ? 27u
            : uint32_t(64 - __builtin_clzll(static_cast<unsigned long long>((~size_t{0} / 4u / sizeof(Cell)) /
                                                                           first_rows)));
    static constexpr uint32_t most_rows = first_rows * ((uint32_t{1} << most_chunks) - 1u);

    ClaimedRows(BlockSource &blocks, Pool pool);
    /* No append or read may still be running. */
    ~ClaimedRows();
    ClaimedRows(const ClaimedRows &) = delete;
    ClaimedRows &operator=(const ClaimedRows &) = delete;
    ClaimedRows(ClaimedRows &&) = delete;
    ClaimedRows &operator=(ClaimedRows &&) = delete;

    /* `row` after every row claimed before it, and ready: no_memory when its
     * chunk cannot be made or the table holds most_rows. */
    [[nodiscard]] Status append(const Row &row);
    /* The first ready row `matches`, or null; the row stays where it is for
     * as long as the table. */
    template <typename Predicate>
    const Row           *find_if(Predicate matches) const;
    /* Every ready row to `visit`, in the order claimed. */
    template <typename Visitor>
    void                 for_each(Visitor visit) const;
    /* Rows claimed, ready or not. */
    uint32_t             claimed() const;

    bool invariants_hold() const;

private:
    uint32_t chunk_of(uint32_t index) const;
    uint32_t first_of(uint32_t chunk) const;
    /* The chunk `chunk`, made and published if no writer has yet; null when
     * it cannot be made. */
    Cell    *chunk(uint32_t chunk);
    /* The ready row at `index`, or null. */
    const Row *ready_at(uint32_t index) const;

    BlockSource &blocks_;
    Pool         pool_;
    Cell        *chunks_[most_chunks];
    uint32_t     claimed_;

    friend struct ClaimedRowsProbe;
};

template <typename Row>
ClaimedRows<Row>::ClaimedRows(BlockSource &blocks, Pool pool)
    : blocks_(blocks), pool_(pool), chunks_{}, claimed_(0u)
{
    AV_SCOPE("ClaimedRows::ClaimedRows");
}

template <typename Row>
ClaimedRows<Row>::~ClaimedRows()
{
    AV_SCOPE("ClaimedRows::~ClaimedRows");
    for (Cell *made : chunks_)
        if (made != nullptr) blocks_.release(made);
}

template <typename Row>
uint32_t ClaimedRows<Row>::chunk_of(uint32_t index) const
{
    AV_SCOPE("ClaimedRows::chunk_of");
    return static_cast<uint32_t>(31 - __builtin_clz(index / first_rows + 1u));
}

template <typename Row>
uint32_t ClaimedRows<Row>::first_of(uint32_t chunk) const
{
    AV_SCOPE("ClaimedRows::first_of");
    return first_rows * ((uint32_t{1} << chunk) - 1u);
}

template <typename Row>
typename ClaimedRows<Row>::Cell *ClaimedRows<Row>::chunk(uint32_t chunk)
{
    AV_SCOPE("ClaimedRows::chunk");
    Cell *published = __atomic_load_n(&chunks_[chunk], __ATOMIC_ACQUIRE);
    if (published != nullptr) return published;
    const size_t cells = size_t{first_rows} << chunk;
    auto *const  made = static_cast<Cell *>(blocks_.allocate(sizeof(Cell) * cells, pool_));
    if (made == nullptr) return nullptr;
    for (size_t index = 0u; index < cells; index++) made[index].ready_ = false;
    if (__atomic_compare_exchange_n(&chunks_[chunk], &published, made, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return made;
    blocks_.release(made);
    return published;
}

template <typename Row>
Status ClaimedRows<Row>::append(const Row &row)
{
    AV_SCOPE("ClaimedRows::append");
    blocks_.require_irql_at_most(Irql::apc, "ClaimedRows::append");
    /* One at a time, so a table at most_rows stays exactly there however
     * many append after. */
    uint32_t index = __atomic_load_n(&claimed_, __ATOMIC_ACQUIRE);
    do {
        if (index >= most_rows) return Status::no_memory;
    } while (!__atomic_compare_exchange_n(&claimed_, &index, index + 1u, true, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
    const uint32_t owner = chunk_of(index);
    Cell *const    cells = chunk(owner);
    if (cells == nullptr) return Status::no_memory;
    Cell &cell = cells[index - first_of(owner)];
    cell.row_ = row;
    __atomic_store_n(&cell.ready_, true, __ATOMIC_RELEASE);
    return Status::ok;
}

template <typename Row>
const Row *ClaimedRows<Row>::ready_at(uint32_t index) const
{
    AV_SCOPE("ClaimedRows::ready_at");
    const uint32_t    owner = chunk_of(index);
    const Cell *const cells = __atomic_load_n(&chunks_[owner], __ATOMIC_ACQUIRE);
    if (cells == nullptr) return nullptr;
    const Cell &cell = cells[index - first_of(owner)];
    return __atomic_load_n(&cell.ready_, __ATOMIC_ACQUIRE) ? &cell.row_ : nullptr;
}

template <typename Row>
template <typename Predicate>
const Row *ClaimedRows<Row>::find_if(Predicate matches) const
{
    AV_SCOPE("ClaimedRows::find_if");
    const uint32_t claimed = this->claimed();
    for (uint32_t index = 0u; index < claimed; index++) {
        const Row *const row = ready_at(index);
        if (row != nullptr && matches(*row)) return row;
    }
    return nullptr;
}

template <typename Row>
template <typename Visitor>
void ClaimedRows<Row>::for_each(Visitor visit) const
{
    AV_SCOPE("ClaimedRows::for_each");
    const uint32_t claimed = this->claimed();
    for (uint32_t index = 0u; index < claimed; index++) {
        const Row *const row = ready_at(index);
        if (row != nullptr) visit(*row);
    }
}

template <typename Row>
uint32_t ClaimedRows<Row>::claimed() const
{
    AV_SCOPE("ClaimedRows::claimed");
    return __atomic_load_n(&claimed_, __ATOMIC_ACQUIRE);
}

/* Only where no writer runs: a ready row in a chunk that does not exist, or
 * past the claim, is what a broken publish would leave. */
template <typename Row>
bool ClaimedRows<Row>::invariants_hold() const
{
    AV_SCOPE("ClaimedRows::invariants_hold");
    const uint32_t claimed = this->claimed();
    if (claimed > most_rows) return false;
    for (uint32_t owner = 0u; owner < most_chunks; owner++) {
        const Cell *const cells = chunks_[owner];
        if (cells == nullptr) continue;
        for (uint32_t index = 0u; index < (first_rows << owner); index++)
            if (cells[index].ready_ && first_of(owner) + index >= claimed) return false;
    }
    return true;
}

}  // namespace av

#endif  // AV_CLAIMED_ROWS_HPP
