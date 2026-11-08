#include "icd/common/present_pacing.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

PresentPacing::PresentPacing() : pacer_(), blanks_(1u), waited_(0u)
{
    AV_SCOPE("PresentPacing::PresentPacing");
}

PresentPacing::~PresentPacing()
{
    AV_SCOPE("PresentPacing::~PresentPacing");
    pacer_.destroy();
}

Status PresentPacing::bind(Kmt &kmt, DriverLog &log, uint32_t adapter, uint32_t device)
{
    AV_SCOPE("PresentPacing::bind");
    return pacer_.construct(kmt, log, adapter, device);
}

void PresentPacing::unbind()
{
    AV_SCOPE("PresentPacing::unbind");
    pacer_.destroy();
}

bool PresentPacing::ask(int32_t blanks)
{
    AV_SCOPE("PresentPacing::ask");
    if (blanks < 0 || static_cast<uint32_t>(blanks) > max_blanks) return false;
    blanks_ = static_cast<uint32_t>(blanks);
    return true;
}

uint32_t PresentPacing::blanks() const
{
    AV_SCOPE("PresentPacing::blanks");
    return blanks_;
}

bool PresentPacing::admits()
{
    AV_SCOPE("PresentPacing::admits");
    if (blanks_ != 0u || !pacer_.constructed()) return true;
    return pacer_.object().admits();
}

uint32_t PresentPacing::next_admit_milliseconds() const
{
    AV_SCOPE("PresentPacing::next_admit_milliseconds");
    return pacer_.constructed() ? pacer_.object().next_blank_milliseconds() : 1u;
}

void PresentPacing::after_present()
{
    AV_SCOPE("PresentPacing::after_present");
    if (!pacer_.constructed()) return;
    for (uint32_t blank = 0u; blank < blanks_; blank++) {
        pacer_.object().wait();
        waited_++;
    }
}

uint64_t PresentPacing::waited() const
{
    AV_SCOPE("PresentPacing::waited");
    return waited_;
}

bool PresentPacing::invariants_hold() const
{
    AV_SCOPE("PresentPacing::invariants_hold");
    if (blanks_ > max_blanks) return false;
    return !pacer_.constructed() || pacer_.object().invariants_hold();
}

}
