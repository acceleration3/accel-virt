#include "icd/d3d/query_kind.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

namespace {
constexpr uint32_t pipeline_statistics_ddi = 8u;
constexpr uint32_t pipeline_statistics_api = 4u;
constexpr uint32_t last_ddi = 16u;
constexpr uint32_t sizes_by_api[] = {4u, 8u, 8u, 16u, 88u, 4u, 16u, 4u, 16u, 16u, 16u, 16u, 4u, 4u, 4u, 4u};
}

QueryKind::QueryKind(uint32_t ddi_type) : api_type_(0u), data_bytes_(0u)
{
    AV_SCOPE("QueryKind::QueryKind");
    if (ddi_type > last_ddi) return;
    api_type_ = ddi_type == pipeline_statistics_ddi ? pipeline_statistics_api : ddi_type > pipeline_statistics_ddi ? ddi_type - 1u : ddi_type;
    data_bytes_ = sizes_by_api[api_type_];
}

bool QueryKind::known() const
{
    AV_SCOPE("QueryKind::known");
    return data_bytes_ != 0u;
}

uint32_t QueryKind::api_type() const
{
    AV_SCOPE("QueryKind::api_type");
    return api_type_;
}

uint32_t QueryKind::data_bytes() const
{
    AV_SCOPE("QueryKind::data_bytes");
    return data_bytes_;
}

}
