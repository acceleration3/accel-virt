#include "icd/d3d/caps_reply.hpp"

#include "core/trace/function_scope.hpp"

#include <string.h>

namespace av::d3d {

CapsReply::CapsReply(void *data, size_t bytes) : data_(data), bytes_(bytes)
{
    AV_SCOPE("CapsReply::CapsReply");
}

bool CapsReply::give_whole(const void *answer, size_t answer_bytes) const
{
    AV_SCOPE("CapsReply::give_whole");
    if (bytes_ < answer_bytes) return false;
    return give_prefix(answer, answer_bytes);
}

bool CapsReply::give_prefix(const void *answer, size_t answer_bytes) const
{
    AV_SCOPE("CapsReply::give_prefix");
    if (!give_none()) return false;
    memcpy(data_, answer, bytes_ < answer_bytes ? bytes_ : answer_bytes);
    return true;
}

bool CapsReply::give_none() const
{
    AV_SCOPE("CapsReply::give_none");
    if (data_ == nullptr) return false;
    memset(data_, 0, bytes_);
    return true;
}

}
