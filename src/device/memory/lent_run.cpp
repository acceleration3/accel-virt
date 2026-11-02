#include "device/memory/lent_run.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

LentRun::LentRun() : lending_(nullptr), offset_(0u), length_(0u)
{
    AV_SCOPE("LentRun::LentRun");
}

LentRun::LentRun(GuestRamLending &lending, uint64_t offset, uint64_t length)
    : lending_(&lending), offset_(offset), length_(length)
{
    AV_SCOPE("LentRun::LentRun");
}

GuestRamLending *LentRun::lending() const
{
    AV_SCOPE("LentRun::lending");
    return lending_;
}

uint64_t LentRun::offset() const
{
    AV_SCOPE("LentRun::offset");
    return offset_;
}

uint64_t LentRun::length() const
{
    AV_SCOPE("LentRun::length");
    return length_;
}

}  // namespace av
