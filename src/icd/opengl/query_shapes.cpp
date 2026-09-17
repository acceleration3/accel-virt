#include "icd/opengl/query_shapes.hpp"

#include "core/trace/function_scope.hpp"
#include "gl_sizes.h"

namespace av::glicd {

QueryShapes::QueryShapes() : lengths_{}
{
    AV_SCOPE("QueryShapes::QueryShapes");
}

void QueryShapes::learn(List list, int32_t length)
{
    AV_SCOPE("QueryShapes::learn");
    lengths_[static_cast<uint32_t>(list)] = length > 0 ? static_cast<uint32_t>(length) : 0u;
}

size_t QueryShapes::values(uint32_t pname) const
{
    AV_SCOPE("QueryShapes::values");
    for (uint32_t list = 0u; list < lists; list++)
        if (list_queries[list] == pname) return lengths_[list];
    return glproto::pname_count(pname);
}

size_t QueryShapes::block_values(uint32_t pname) const
{
    AV_SCOPE("QueryShapes::block_values");
    return pname == active_uniform_indices ? 0u : 1u;
}

/* A learned length is at most what a GLint can say. */
bool QueryShapes::invariants_hold() const
{
    AV_SCOPE("QueryShapes::invariants_hold");
    for (const uint32_t length : lengths_)
        if (length > 0x7FFFFFFFu) return false;
    return true;
}

}  // namespace av::glicd
