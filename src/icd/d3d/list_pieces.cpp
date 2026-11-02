#include "icd/d3d/list_pieces.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

ListPieces::ListPieces(uint64_t total, uint64_t most)
    : total_(total), most_(most), offset_(0u), count_(0u), sent_(0u)
{
    AV_SCOPE("ListPieces::ListPieces");
}

bool ListPieces::next()
{
    AV_SCOPE("ListPieces::next");
    if (most_ == 0u) return false;
    offset_ += count_;
    if (sent_ != 0u && offset_ >= total_) return false;
    count_ = total_ - offset_ < most_ ? total_ - offset_ : most_;
    sent_++;
    return true;
}

uint64_t ListPieces::offset() const
{
    AV_SCOPE("ListPieces::offset");
    return offset_;
}

uint64_t ListPieces::count() const
{
    AV_SCOPE("ListPieces::count");
    return count_;
}

}
