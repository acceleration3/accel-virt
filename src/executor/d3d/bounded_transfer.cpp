#include "executor/d3d/bounded_transfer.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

BoundedTransfer::BoundedTransfer(uint64_t units, uint64_t unit_bytes)
    : units_(units), per_piece_(unit_bytes == 0u ? (units == 0u ? 1u : units) : unit_bytes >= most_bytes ? 1u : most_bytes / unit_bytes),
      done_(0u)
{
    AV_SCOPE("BoundedTransfer::BoundedTransfer");
}

bool BoundedTransfer::split() const
{
    AV_SCOPE("BoundedTransfer::split");
    return units_ > per_piece_;
}

bool BoundedTransfer::next(uint64_t &first, uint64_t &count)
{
    AV_SCOPE("BoundedTransfer::next");
    if (done_ >= units_) return false;
    first = done_;
    count = units_ - done_ < per_piece_ ? units_ - done_ : per_piece_;
    done_ += count;
    return true;
}

bool BoundedTransfer::invariants_hold() const
{
    AV_SCOPE("BoundedTransfer::invariants_hold");
    return per_piece_ >= 1u && done_ <= units_;
}

}
