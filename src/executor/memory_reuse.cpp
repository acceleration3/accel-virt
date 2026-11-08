#include "executor/memory_reuse.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

MemoryReuse::MemoryReuse() : last_{}
{
    AV_SCOPE("MemoryReuse::MemoryReuse");
    for (uint32_t &entry : last_) entry = never_freed;
}

void MemoryReuse::freed(uint32_t type, uint32_t domain)
{
    AV_SCOPE("MemoryReuse::freed");
    if (type >= type_count) return;
    if (last_[type] == never_freed) last_[type] = domain;
    else if (last_[type] != domain) last_[type] = freed_by_several;
}

bool MemoryReuse::crosses_domains(uint32_t type, uint32_t domain) const
{
    AV_SCOPE("MemoryReuse::crosses_domains");
    if (type >= type_count) return true;
    return last_[type] != never_freed && last_[type] != domain;
}

}
