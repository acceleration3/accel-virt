#include "icd/common/pieces.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

Pieces::Pieces(uint64_t total, uint64_t most) : total_(total), most_(most), offset_(0u), count_(0u)
{
    AV_SCOPE("Pieces::Pieces");
}

bool Pieces::next()
{
    AV_SCOPE("Pieces::next");
    offset_ += count_;
    if (offset_ >= total_ || most_ == 0u) return false;
    count_ = total_ - offset_ < most_ ? total_ - offset_ : most_;
    return true;
}

uint64_t Pieces::offset() const
{
    AV_SCOPE("Pieces::offset");
    return offset_;
}

uint64_t Pieces::count() const
{
    AV_SCOPE("Pieces::count");
    return count_;
}

bool Pieces::first() const
{
    AV_SCOPE("Pieces::first");
    return offset_ == 0u;
}

}
