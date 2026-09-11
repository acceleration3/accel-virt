#include "adapter_info_query.hpp"

#include "ddi/dxgkarg_query_adapter_info_record.hpp"

namespace av {

AdapterInfoQuery::AdapterInfoQuery(Platform &platform, const DxgkargQueryAdapterInfoRecord &record)
    : platform_(platform), record_(record) {}

int32_t AdapterInfoQuery::type() const
{
    platform_.require_irql_at_most(Irql::passive, "AdapterInfoQuery::type");
    return record_.Type;
}

Status AdapterInfoQuery::require_input(uint32_t bytes) const
{
    platform_.require_irql_at_most(Irql::passive, "AdapterInfoQuery::require_input");
    if (bytes == 0u) return Status::ok;
    if (record_.pInputData == nullptr || record_.InputDataSize < bytes) return Status::invalid_parameter;
    return Status::ok;
}

Status AdapterInfoQuery::read_input(void *destination, uint32_t count) const
{
    platform_.require_irql_at_most(Irql::passive, "AdapterInfoQuery::read_input");
    const Status status = require_input(count);
    if (status == Status::ok) platform_.copy(destination, record_.pInputData, count);
    return status;
}

Status AdapterInfoQuery::read_output(void *destination, uint32_t count) const
{
    platform_.require_irql_at_most(Irql::passive, "AdapterInfoQuery::read_output");
    if (record_.pOutputData == nullptr || record_.OutputDataSize < count) return Status::invalid_parameter;
    platform_.copy(destination, record_.pOutputData, count);
    return Status::ok;
}

Status AdapterInfoQuery::write_output(const void *bytes, uint32_t count) const
{
    platform_.require_irql_at_most(Irql::passive, "AdapterInfoQuery::write_output");
    if (record_.pOutputData == nullptr || record_.OutputDataSize < count) return Status::invalid_parameter;
    platform_.copy(record_.pOutputData, bytes, count);
    return Status::ok;
}

}  // namespace av
