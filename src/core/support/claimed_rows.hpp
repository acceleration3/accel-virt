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
    static constexpr uint32_t first_rows = 16u;
    static constexpr uint32_t most_chunks =
        (~size_t{0} / 4u / sizeof(Cell)) / first_rows >= (size_t{1} << 26u)
            ? 27u
            : uint32_t(64 - __builtin_clzll(static_cast<unsigned long long>((~size_t{0} / 4u / sizeof(Cell)) /
                                                                           first_rows)));
    static constexpr uint32_t most_rows = first_rows * ((uint32_t{1} << most_chunks) - 1u);

    ClaimedRows(BlockSource &blocks, Pool pool);
    ~ClaimedRows();
    ClaimedRows(const ClaimedRows &) = delete;
    ClaimedRows &operator=(const ClaimedRows &) = delete;
    ClaimedRows(ClaimedRows &&) = delete;
    ClaimedRows &operator=(ClaimedRows &&) = delete;

    [[nodiscard]] Status append(const Row &row);
    template <typename Predicate>
    const Row           *find_if(Predicate matches) const;
    template <typename Visitor>
    void                 for_each(Visitor visit) const;
    uint32_t             claimed() const;
    uint32_t             ready() const;

    bool invariants_hold() const;

private:
    uint32_t chunk_of(uint32_t index) const;
    uint32_t first_of(uint32_t chunk) const;
    Cell    *chunk(uint32_t chunk);
    const Row *ready_at(uint32_t index) const;

    BlockSource &blocks_;
    Pool         pool_;
    Cell        *chunks_[most_chunks];
    uint32_t     claimed_;
    uint32_t     ready_;

    friend struct ClaimedRowsProbe;
};

template <typename Row>
ClaimedRows<Row>::ClaimedRows(BlockSource &blocks, Pool pool)
    : blocks_(blocks), pool_(pool), chunks_{}, claimed_(0u), ready_(0u)
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
    __atomic_fetch_add(&ready_, 1u, __ATOMIC_RELEASE);
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

template <typename Row>
uint32_t ClaimedRows<Row>::ready() const
{
    AV_SCOPE("ClaimedRows::ready");
    return __atomic_load_n(&ready_, __ATOMIC_ACQUIRE);
}

template <typename Row>
bool ClaimedRows<Row>::invariants_hold() const
{
    AV_SCOPE("ClaimedRows::invariants_hold");
    const uint32_t claimed = this->claimed();
    if (claimed > most_rows || ready() > claimed) return false;
    for (uint32_t owner = 0u; owner < most_chunks; owner++) {
        const Cell *const cells = chunks_[owner];
        if (cells == nullptr) continue;
        for (uint32_t index = 0u; index < (first_rows << owner); index++)
            if (cells[index].ready_ && first_of(owner) + index >= claimed) return false;
    }
    return true;
}

}

#endif
