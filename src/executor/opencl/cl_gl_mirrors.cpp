#include "executor/opencl/cl_gl_mirrors.hpp"

#include "core/trace/function_scope.hpp"

#include <sys/mman.h>

namespace av {

ClGlMirrors::ClGlMirrors() : table_(nullptr), mirrors_(nullptr), index_(nullptr), count_(0u)
{
    AV_SCOPE("ClGlMirrors::ClGlMirrors");
    static_assert((buckets & (buckets - 1u)) == 0u);
    static_assert(sizeof(GlMirror) % alignof(uint32_t) == 0u);
}

ClGlMirrors::~ClGlMirrors()
{
    AV_SCOPE("ClGlMirrors::~ClGlMirrors");
    forget_all();
}

bool ClGlMirrors::remember(const GlMirror &mirror)
{
    AV_SCOPE("ClGlMirrors::remember");
    if (!mirror.described() || count_ == max_mirrors || !reserve()) return false;
    const uint32_t bucket = bucket_of(mirror.mem());
    if (index_[bucket] != 0u) return false;
    mirrors_[count_] = mirror;
    index_[bucket] = ++count_;
    return true;
}

GlMirror *ClGlMirrors::find(const void *mem)
{
    AV_SCOPE("ClGlMirrors::find");
    const uint32_t slot = slot_of(mem);
    return slot != max_mirrors ? &mirrors_[slot] : nullptr;
}

bool ClGlMirrors::forget(const void *mem, GlMirror &gone)
{
    AV_SCOPE("ClGlMirrors::forget");
    const uint32_t slot = slot_of(mem);
    if (slot == max_mirrors) return false;
    forget_slot(slot, gone);
    return true;
}

bool ClGlMirrors::forget_any(GlMirror &gone)
{
    AV_SCOPE("ClGlMirrors::forget_any");
    if (count_ == 0u) return false;
    forget_slot(count_ - 1u, gone);
    return true;
}

void ClGlMirrors::forget_all()
{
    AV_SCOPE("ClGlMirrors::forget_all");
    if (table_ != nullptr) static_cast<void>(munmap(table_, table_bytes));
    table_ = nullptr;
    mirrors_ = nullptr;
    index_ = nullptr;
    count_ = 0u;
}

uint32_t ClGlMirrors::count() const
{
    AV_SCOPE("ClGlMirrors::count");
    return count_;
}

bool ClGlMirrors::reserve()
{
    AV_SCOPE("ClGlMirrors::reserve");
    if (table_ != nullptr) return true;
    void *const mapped = mmap(nullptr, table_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                              -1, 0);
    if (mapped == MAP_FAILED) return false;
    table_ = mapped;
    mirrors_ = static_cast<GlMirror *>(mapped);
    index_ = reinterpret_cast<uint32_t *>(mirrors_ + max_mirrors);
    return true;
}

uint32_t ClGlMirrors::slot_of(const void *mem) const
{
    AV_SCOPE("ClGlMirrors::slot_of");
    if (table_ == nullptr) return max_mirrors;
    const uint32_t entry = index_[bucket_of(mem)];
    return entry != 0u ? entry - 1u : max_mirrors;
}

uint32_t ClGlMirrors::bucket_of(const void *mem) const
{
    AV_SCOPE("ClGlMirrors::bucket_of");
    constexpr uint32_t mask = buckets - 1u;
    uint32_t           bucket = home_of(mem);
    while (index_[bucket] != 0u && mirrors_[index_[bucket] - 1u].mem() != mem) bucket = (bucket + 1u) & mask;
    return bucket;
}

uint32_t ClGlMirrors::home_of(const void *mem) const
{
    AV_SCOPE("ClGlMirrors::home_of");
    const uint64_t address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(mem));
    return static_cast<uint32_t>((address * spread) >> 32u) & (buckets - 1u);
}

void ClGlMirrors::unindex(uint32_t bucket)
{
    AV_SCOPE("ClGlMirrors::unindex");
    constexpr uint32_t mask = buckets - 1u;
    uint32_t           hole = bucket;
    for (uint32_t next = (hole + 1u) & mask; index_[next] != 0u; next = (next + 1u) & mask) {
        /* An entry moves into the hole only if the hole is on its probe path:
         * between its home bucket and where it sits now. */
        const uint32_t home = home_of(mirrors_[index_[next] - 1u].mem());
        if (((next - home) & mask) < ((next - hole) & mask)) continue;
        index_[hole] = index_[next];
        hole = next;
    }
    index_[hole] = 0u;
}

/* The last held moves into the hole, so the held stay the first count_; its
 * bucket is found while its old slot still holds it. */
void ClGlMirrors::forget_slot(uint32_t slot, GlMirror &gone)
{
    AV_SCOPE("ClGlMirrors::forget_slot");
    gone = mirrors_[slot];
    unindex(bucket_of(gone.mem()));
    count_--;
    if (slot == count_) return;
    const uint32_t moved = bucket_of(mirrors_[count_].mem());
    mirrors_[slot] = mirrors_[count_];
    index_[moved] = slot + 1u;
}

/* Every held slot is a mirror of something, found at itself - so no mem is
 * held twice - and the index holds the held and nothing else. */
bool ClGlMirrors::invariants_hold() const
{
    AV_SCOPE("ClGlMirrors::invariants_hold");
    if (table_ == nullptr) return count_ == 0u;
    if (count_ > max_mirrors) return false;
    for (uint32_t slot = 0u; slot < count_; slot++)
        if (!mirrors_[slot].described() || slot_of(mirrors_[slot].mem()) != slot) return false;
    uint32_t indexed = 0u;
    for (uint32_t bucket = 0u; bucket < buckets; bucket++) {
        if (index_[bucket] > count_) return false;
        indexed += index_[bucket] != 0u ? 1u : 0u;
    }
    return indexed == count_;
}

}  // namespace av
