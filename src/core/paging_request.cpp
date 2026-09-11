#include "paging_request.hpp"

#include "ddi/dxgkarg_build_paging_buffer_record.hpp"

namespace av {

PagingRequest::PagingRequest(Platform &platform, DxgkargBuildPagingBufferRecord &request)
    : platform_(platform), request_(request) {}

Status PagingRequest::build()
{
    platform_.require_irql_at_most(Irql::passive, "PagingRequest::build");
    const int32_t operation = request_.Operation;
    const bool aperture = operation == discard_content || operation == map_aperture_segment ||
                          operation == unmap_aperture_segment;
    return aperture ? Status::ok : Status::invalid_parameter;
}

}  // namespace av
