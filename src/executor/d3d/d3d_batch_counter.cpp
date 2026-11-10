#include "executor/d3d/d3d_batch_counter.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

bool D3dBatchCounter::Marks::covered_by(const Marks &done) const
{
    AV_SCOPE("D3dBatchCounter::Marks::covered_by");
    return d3d11_ <= done.d3d11_ && d3d12_ <= done.d3d12_ && d3d9_ <= done.d3d9_ &&
           d3d9_lent_ <= done.d3d9_lent_;
}

D3dBatchCounter::D3dBatchCounter() : pending_(), first_(0u), count_(0u), last_(), marked_(0u)
{
    AV_SCOPE("D3dBatchCounter::D3dBatchCounter");
}

const D3dBatchCounter::Record &D3dBatchCounter::at(uint32_t index) const
{
    AV_SCOPE("D3dBatchCounter::at");
    return pending_[(first_ + index) % pending_capacity];
}

D3dBatchCounter::Record &D3dBatchCounter::newest()
{
    AV_SCOPE("D3dBatchCounter::newest");
    return pending_[(first_ + count_ - 1u) % pending_capacity];
}

void D3dBatchCounter::retire(const Marks &done)
{
    AV_SCOPE("D3dBatchCounter::retire");
    while (count_ != 0u && at(0u).marks_.covered_by(done)) {
        first_ = (first_ + 1u) % pending_capacity;
        count_--;
    }
}

void D3dBatchCounter::end(const Marks &issued, const Marks &done)
{
    AV_SCOPE("D3dBatchCounter::end");
    marked_++;
    retire(done);
    const bool unchanged = issued == last_;
    last_ = issued;
    if (unchanged || issued.covered_by(done)) return;
    if (count_ == pending_capacity) {
        newest().marks_ = issued;
        return;
    }
    pending_[(first_ + count_) % pending_capacity] = Record{marked_, issued};
    count_++;
}

uint64_t D3dBatchCounter::marked() const
{
    AV_SCOPE("D3dBatchCounter::marked");
    return marked_;
}

uint64_t D3dBatchCounter::reached(const Marks &done) const
{
    AV_SCOPE("D3dBatchCounter::reached");
    for (uint32_t index = 0u; index < count_; index++)
        if (!at(index).marks_.covered_by(done)) return at(index).from_ - 1u;
    return marked_;
}

bool D3dBatchCounter::pending(const Marks &done) const
{
    AV_SCOPE("D3dBatchCounter::pending");
    return reached(done) < marked_;
}

bool D3dBatchCounter::invariants_hold() const
{
    AV_SCOPE("D3dBatchCounter::invariants_hold");
    if (count_ > pending_capacity || first_ >= pending_capacity) return false;
    uint64_t from = 0u;
    for (uint32_t index = 0u; index < count_; index++) {
        const Record &held = at(index);
        if (held.from_ <= from || held.from_ > marked_) return false;
        if (index != 0u && !at(index - 1u).marks_.covered_by(held.marks_)) return false;
        from = held.from_;
    }
    return true;
}

}
