#include "miniport/memory/point_view.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

PointView::PointView() : pointed_(false), queued_(false), point_(0u), first_starvation_(false)
{
    AV_SCOPE("PointView::PointView");
}

PointView::PointView(bool pointed, bool queued, uint32_t point, bool first_starvation)
    : pointed_(pointed), queued_(queued), point_(point), first_starvation_(first_starvation)
{
    AV_SCOPE("PointView::PointView");
}

bool PointView::pointed() const
{
    AV_SCOPE("PointView::pointed");
    return pointed_;
}

bool PointView::queued() const
{
    AV_SCOPE("PointView::queued");
    return queued_;
}

uint32_t PointView::point() const
{
    AV_SCOPE("PointView::point");
    return point_;
}

bool PointView::first_starvation() const
{
    AV_SCOPE("PointView::first_starvation");
    return first_starvation_;
}

}
