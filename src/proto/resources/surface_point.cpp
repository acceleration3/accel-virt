#include "proto/resources/surface_point.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

SurfacePoint::SurfacePoint() : surface_(0u), point_(0u)
{
    AV_SCOPE("SurfacePoint::SurfacePoint");
}

SurfacePoint::SurfacePoint(uint64_t surface, uint32_t point) : surface_(surface), point_(point)
{
    AV_SCOPE("SurfacePoint::SurfacePoint");
}

uint64_t SurfacePoint::surface() const
{
    AV_SCOPE("SurfacePoint::surface");
    return surface_;
}

uint32_t SurfacePoint::point() const
{
    AV_SCOPE("SurfacePoint::point");
    return point_;
}

bool SurfacePoint::names() const
{
    AV_SCOPE("SurfacePoint::names");
    return surface_ != 0u && point_ != 0u;
}

bool SurfacePoint::operator==(const SurfacePoint &other) const
{
    AV_SCOPE("SurfacePoint::operator==");
    return surface_ == other.surface_ && point_ == other.point_;
}

}  // namespace av
