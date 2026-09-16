#include "core/miniport/adapter_info_query.hpp"

#include "core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

AdapterInfoQuery::AdapterInfoQuery(Platform &platform, const DxgkargQueryAdapterInfoRecord &record)
    : platform_(platform), record_(record)
    {
        AV_SCOPE("AdapterInfoQuery::AdapterInfoQuery");
    }

int32_t AdapterInfoQuery::type() const
{
    AV_SCOPE("AdapterInfoQuery::type");
    platform_.require_irql_at_most(Irql::passive, "AdapterInfoQuery::type");
    return record_.Type;
}

Status AdapterInfoQuery::require_input(uint32_t bytes) const
{
    AV_SCOPE("AdapterInfoQuery::require_input");
    platform_.require_irql_at_most(Irql::passive, "AdapterInfoQuery::require_input");
    if (bytes == 0u) return Status::ok;
    if (record_.pInputData == nullptr || record_.InputDataSize < bytes) return Status::invalid_parameter;
    return Status::ok;
}

Status AdapterInfoQuery::read_input(void *destination, uint32_t count) const
{
    AV_SCOPE("AdapterInfoQuery::read_input");
    platform_.require_irql_at_most(Irql::passive, "AdapterInfoQuery::read_input");
    const Status status = require_input(count);
    if (status == Status::ok) platform_.copy(destination, record_.pInputData, count);
    return status;
}

Status AdapterInfoQuery::read_output(void *destination, uint32_t count) const
{
    AV_SCOPE("AdapterInfoQuery::read_output");
    platform_.require_irql_at_most(Irql::passive, "AdapterInfoQuery::read_output");
    if (record_.pOutputData == nullptr || record_.OutputDataSize < count) return Status::invalid_parameter;
    platform_.copy(destination, record_.pOutputData, count);
    return Status::ok;
}

Status AdapterInfoQuery::write_output(const void *bytes, uint32_t count) const
{
    AV_SCOPE("AdapterInfoQuery::write_output");
    platform_.require_irql_at_most(Irql::passive, "AdapterInfoQuery::write_output");
    if (record_.pOutputData == nullptr || record_.OutputDataSize < count) return Status::invalid_parameter;
    platform_.copy(record_.pOutputData, bytes, count);
    return Status::ok;
}

}  // namespace av
