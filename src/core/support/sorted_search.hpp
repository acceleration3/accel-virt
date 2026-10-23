#ifndef AV_SORTED_SEARCH_HPP
#define AV_SORTED_SEARCH_HPP

#include "core/support/growable_array.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

// A view of rows ordered by a numeric key. The owner keeps them sorted;
// searches allocate nothing and remain valid when the array grows.
template <typename Row, auto Key = nullptr>
class SortedSearch {
public:
    explicit SortedSearch(const GrowableArray<Row> &rows);

    uint32_t at_or_after(uint64_t key) const;
    uint32_t above(uint64_t key) const;
    // No predecessor or exact match is represented by rows.size().
    uint32_t at_or_below(uint64_t key) const;
    uint32_t exact(uint64_t key) const;
    bool invariants_hold() const;

private:
    uint64_t key_at(uint32_t row) const;
    const GrowableArray<Row> &rows_;
};

template <typename Row, auto Key>
SortedSearch<Row, Key>::SortedSearch(const GrowableArray<Row> &rows) : rows_(rows)
{
    AV_SCOPE("SortedSearch::SortedSearch");
}

template <typename Row, auto Key>
uint64_t SortedSearch<Row, Key>::key_at(uint32_t row) const
{
    AV_SCOPE("SortedSearch::key_at");
    if constexpr (__is_same(decltype(Key), decltype(nullptr))) return rows_.at(row);
    else if constexpr (__is_member_object_pointer(decltype(Key))) return rows_.at(row).*Key;
    else return Key(rows_.at(row));
}

template <typename Row, auto Key>
uint32_t SortedSearch<Row, Key>::at_or_after(uint64_t key) const
{
    AV_SCOPE("SortedSearch::at_or_after");
    uint32_t low = 0u;
    uint32_t high = rows_.size();
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2u;
        if (key_at(middle) < key) low = middle + 1u;
        else high = middle;
    }
    return low;
}

template <typename Row, auto Key>
uint32_t SortedSearch<Row, Key>::above(uint64_t key) const
{
    AV_SCOPE("SortedSearch::above");
    return key == UINT64_MAX ? rows_.size() : at_or_after(key + 1u);
}

template <typename Row, auto Key>
uint32_t SortedSearch<Row, Key>::at_or_below(uint64_t key) const
{
    AV_SCOPE("SortedSearch::at_or_below");
    const uint32_t after = above(key);
    return after == 0u ? rows_.size() : after - 1u;
}

template <typename Row, auto Key>
uint32_t SortedSearch<Row, Key>::exact(uint64_t key) const
{
    AV_SCOPE("SortedSearch::exact");
    const uint32_t row = at_or_after(key);
    return row < rows_.size() && key_at(row) == key ? row : rows_.size();
}

template <typename Row, auto Key>
bool SortedSearch<Row, Key>::invariants_hold() const
{
    AV_SCOPE("SortedSearch::invariants_hold");
    if (!rows_.invariants_hold()) return false;
    for (uint32_t row = 1u; row < rows_.size(); row++)
        if (key_at(row - 1u) > key_at(row)) return false;
    return true;
}

}  // namespace av

#endif  // AV_SORTED_SEARCH_HPP
