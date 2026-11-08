#include "proto/resources/surface_pair.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

SurfacePair::SurfacePair() : source_(0u), destination_(0u)
{
    AV_SCOPE("SurfacePair::SurfacePair");
}

SurfacePair::SurfacePair(uint64_t source, uint64_t destination) : source_(source), destination_(destination)
{
    AV_SCOPE("SurfacePair::SurfacePair");
}

uint64_t SurfacePair::source() const
{
    AV_SCOPE("SurfacePair::source");
    return source_;
}

uint64_t SurfacePair::destination() const
{
    AV_SCOPE("SurfacePair::destination");
    return destination_;
}

bool SurfacePair::names() const
{
    AV_SCOPE("SurfacePair::names");
    return source_ != 0u && destination_ != 0u;
}

bool SurfacePair::operator==(const SurfacePair &other) const
{
    AV_SCOPE("SurfacePair::operator==");
    return source_ == other.source_ && destination_ == other.destination_;
}

}
