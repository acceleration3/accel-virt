#include "icd/d3d/byte_pieces.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

BytePieces::BytePieces(uint64_t length, uint64_t most) : length_(length), most_(most), offset_(0u), bytes_(0u)
{
    AV_SCOPE("BytePieces::BytePieces");
}

bool BytePieces::next()
{
    AV_SCOPE("BytePieces::next");
    offset_ += bytes_;
    if (offset_ >= length_ || most_ == 0u) return false;
    bytes_ = length_ - offset_ < most_ ? length_ - offset_ : most_;
    return true;
}

uint64_t BytePieces::offset() const
{
    AV_SCOPE("BytePieces::offset");
    return offset_;
}

uint64_t BytePieces::bytes() const
{
    AV_SCOPE("BytePieces::bytes");
    return bytes_;
}

bool BytePieces::first() const
{
    AV_SCOPE("BytePieces::first");
    return offset_ == 0u;
}

}  // namespace av::d3d
