#include "icd/opencl/cl_format_cache.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClFormatCache::ClFormatCache() : entries_{}
{
    AV_SCOPE("ClFormatCache::ClFormatCache");
}

const cl_image_format *ClFormatCache::find(cl_mem_flags flags, cl_mem_object_type type, uint32_t &count) const
{
    AV_SCOPE("ClFormatCache::find");
    count = 0u;
    for (const Entry &entry : entries_) {
        if (!entry.used_ || entry.flags_ != flags || entry.type_ != type) continue;
        count = entry.count_;
        return entry.formats_;
    }
    return nullptr;
}

bool ClFormatCache::keep(cl_mem_flags flags, cl_mem_object_type type, const cl_image_format *formats, uint32_t count)
{
    AV_SCOPE("ClFormatCache::keep");
    if (count > most_formats) return false;
    for (Entry &entry : entries_) {
        if (entry.used_) continue;
        entry.used_ = true;
        entry.flags_ = flags;
        entry.type_ = type;
        entry.count_ = count;
        for (uint32_t index = 0u; index < count; ++index) entry.formats_[index] = formats[index];
        return true;
    }
    return false;
}

}
