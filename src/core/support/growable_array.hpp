/* A run of values whose count has no fixed bound - a guest's handles, its
 * mapped ranges, a sorted table of address ranges - held contiguously in one
 * block from a BlockSource, which doubles when the run outgrows it. Indexing
 * is a plain array's; an address into the run is good only until the next
 * call that can grow it, so a caller that hands out pointers keeps its objects
 * in an ObjectPool and only their pointers here.
 *
 * Values are copied bytewise when the block moves, so T is trivially
 * copyable: nothing here constructs or destroys one, and no placement-new is
 * needed (constitution III).
 *
 * The array takes no lock; its owner serializes it. Growing allocates, so
 * every call that can grow is legal up to APC; reading is legal anywhere. */
#ifndef AV_GROWABLE_ARRAY_HPP
#define AV_GROWABLE_ARRAY_HPP

#include "core/platform/block_source.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct GrowableArrayProbe;

template <typename T>
class GrowableArray {
    static_assert(__is_trivially_copyable(T), "GrowableArray moves its values bytewise");

public:
    /* The first block's room, and the least any later one has. */
    static constexpr uint32_t first_capacity = 8u;
    /* The most values a block may hold: a power of two whose bytes a size_t
     * still counts, on the 32-bit guest builds as on the host. */
    static constexpr uint32_t most_values =
        size_t{0x80000000u} <= ~size_t{0} / sizeof(T)
            ? 0x80000000u
            : uint32_t{1} << (63 - __builtin_clzll(static_cast<unsigned long long>(~size_t{0} / sizeof(T))));

    GrowableArray(BlockSource &blocks, Pool pool);
    ~GrowableArray();
    GrowableArray(const GrowableArray &) = delete;
    GrowableArray &operator=(const GrowableArray &) = delete;
    GrowableArray(GrowableArray &&) = delete;
    GrowableArray &operator=(GrowableArray &&) = delete;

    /* `value` after the last; no_memory, with nothing changed, when the block
     * cannot grow. */
    [[nodiscard]] Status append(const T &value);
    /* `value` at `index`, the values from there on one further along:
     * invalid_parameter past the end, no_memory when the block cannot grow. */
    [[nodiscard]] Status insert_at(uint32_t index, const T &value);
    /* Room for `count` values, so appends up to it cannot fail. */
    [[nodiscard]] Status reserve(uint32_t count);
    /* `count` values, those past the old end set to `fill`. */
    [[nodiscard]] Status resize(uint32_t count, const T &fill);
    /* The value at `index` gone, those after it one nearer; nothing past the end. */
    void                 remove_at(uint32_t index);
    /* The value at `index` gone and the last in its place: order is not kept,
     * nothing else moves. Nothing past the end. */
    void                 remove_swap(uint32_t index);
    /* The values from `count` on gone; the block is kept. */
    void                 truncate(uint32_t count);
    /* Every value gone and the block given back. */
    void                 clear();

    /* `index` is below size(): callers bound what they were handed. */
    T       &at(uint32_t index);
    const T &at(uint32_t index) const;
    T       *begin();
    T       *end();
    const T *begin() const;
    const T *end() const;
    uint32_t size() const;
    uint32_t capacity() const;

    bool invariants_hold() const;

private:
    /* A block of at least `count`, the values moved into it. */
    [[nodiscard]] Status grow_to(uint32_t count);

    BlockSource &blocks_;
    Pool         pool_;
    T           *values_;
    uint32_t     size_;
    uint32_t     capacity_;

    friend struct GrowableArrayProbe;
};

template <typename T>
GrowableArray<T>::GrowableArray(BlockSource &blocks, Pool pool)
    : blocks_(blocks), pool_(pool), values_(nullptr), size_(0u), capacity_(0u)
{
    AV_SCOPE("GrowableArray::GrowableArray");
}

template <typename T>
GrowableArray<T>::~GrowableArray()
{
    AV_SCOPE("GrowableArray::~GrowableArray");
    clear();
}

template <typename T>
Status GrowableArray<T>::grow_to(uint32_t count)
{
    AV_SCOPE("GrowableArray::grow_to");
    if (count <= capacity_) return Status::ok;
    if (count > most_values) return Status::no_memory;
    uint32_t wanted = capacity_ < first_capacity ? first_capacity : capacity_;
    while (wanted < count) wanted *= 2u;
    auto *const grown = static_cast<T *>(blocks_.allocate(sizeof(T) * size_t{wanted}, pool_));
    if (grown == nullptr) return Status::no_memory;
    if (size_ != 0u) __builtin_memcpy(static_cast<void *>(grown), values_, sizeof(T) * size_t{size_});
    if (values_ != nullptr) blocks_.release(values_);
    values_ = grown;
    capacity_ = wanted;
    return Status::ok;
}

template <typename T>
Status GrowableArray<T>::append(const T &value)
{
    AV_SCOPE("GrowableArray::append");
    blocks_.require_irql_at_most(Irql::apc, "GrowableArray::append");
    const Status grown = grow_to(size_ + 1u);
    if (grown != Status::ok) return grown;
    values_[size_++] = value;
    return Status::ok;
}

template <typename T>
Status GrowableArray<T>::insert_at(uint32_t index, const T &value)
{
    AV_SCOPE("GrowableArray::insert_at");
    blocks_.require_irql_at_most(Irql::apc, "GrowableArray::insert_at");
    if (index > size_) return Status::invalid_parameter;
    const Status grown = grow_to(size_ + 1u);
    if (grown != Status::ok) return grown;
    __builtin_memmove(static_cast<void *>(values_ + index + 1u), values_ + index, sizeof(T) * size_t{size_ - index});
    values_[index] = value;
    size_++;
    return Status::ok;
}

template <typename T>
Status GrowableArray<T>::reserve(uint32_t count)
{
    AV_SCOPE("GrowableArray::reserve");
    blocks_.require_irql_at_most(Irql::apc, "GrowableArray::reserve");
    return grow_to(count);
}

template <typename T>
Status GrowableArray<T>::resize(uint32_t count, const T &fill)
{
    AV_SCOPE("GrowableArray::resize");
    blocks_.require_irql_at_most(Irql::apc, "GrowableArray::resize");
    const Status grown = grow_to(count);
    if (grown != Status::ok) return grown;
    for (uint32_t index = size_; index < count; index++) values_[index] = fill;
    size_ = count;
    return Status::ok;
}

template <typename T>
void GrowableArray<T>::remove_at(uint32_t index)
{
    AV_SCOPE("GrowableArray::remove_at");
    if (index >= size_) return;
    __builtin_memmove(static_cast<void *>(values_ + index), values_ + index + 1u,
                      sizeof(T) * size_t{size_ - index - 1u});
    size_--;
}

template <typename T>
void GrowableArray<T>::remove_swap(uint32_t index)
{
    AV_SCOPE("GrowableArray::remove_swap");
    if (index >= size_) return;
    values_[index] = values_[size_ - 1u];
    size_--;
}

template <typename T>
void GrowableArray<T>::truncate(uint32_t count)
{
    AV_SCOPE("GrowableArray::truncate");
    if (count < size_) size_ = count;
}

template <typename T>
void GrowableArray<T>::clear()
{
    AV_SCOPE("GrowableArray::clear");
    blocks_.require_irql_at_most(Irql::apc, "GrowableArray::clear");
    if (values_ != nullptr) blocks_.release(values_);
    values_ = nullptr;
    size_ = 0u;
    capacity_ = 0u;
}

template <typename T>
T &GrowableArray<T>::at(uint32_t index)
{
    AV_SCOPE("GrowableArray::at");
    return values_[index];
}

template <typename T>
const T &GrowableArray<T>::at(uint32_t index) const
{
    AV_SCOPE("GrowableArray::at");
    return values_[index];
}

template <typename T>
T *GrowableArray<T>::begin()
{
    AV_SCOPE("GrowableArray::begin");
    return values_;
}

template <typename T>
T *GrowableArray<T>::end()
{
    AV_SCOPE("GrowableArray::end");
    return values_ + size_;
}

template <typename T>
const T *GrowableArray<T>::begin() const
{
    AV_SCOPE("GrowableArray::begin");
    return values_;
}

template <typename T>
const T *GrowableArray<T>::end() const
{
    AV_SCOPE("GrowableArray::end");
    return values_ + size_;
}

template <typename T>
uint32_t GrowableArray<T>::size() const
{
    AV_SCOPE("GrowableArray::size");
    return size_;
}

template <typename T>
uint32_t GrowableArray<T>::capacity() const
{
    AV_SCOPE("GrowableArray::capacity");
    return capacity_;
}

template <typename T>
bool GrowableArray<T>::invariants_hold() const
{
    AV_SCOPE("GrowableArray::invariants_hold");
    if (values_ == nullptr) return size_ == 0u && capacity_ == 0u;
    return size_ <= capacity_ && capacity_ >= first_capacity && capacity_ <= most_values &&
           (capacity_ & (capacity_ - 1u)) == 0u;
}

}  // namespace av

#endif  // AV_GROWABLE_ARRAY_HPP
