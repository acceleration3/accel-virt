#include "icd/common/loss_latch.hpp"

#include "core/log/keyed_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::icd {

LossLatch::LossLatch(KeyedLog &lines) : lines_(lines), word_(0u)
{
    AV_SCOPE("LossLatch::LossLatch");
}

void LossLatch::lose(FaultCode cause)
{
    AV_SCOPE("LossLatch::lose");
    uint32_t live = 0u;
    static_cast<void>(__atomic_compare_exchange_n(&word_, &live, lost_bit | static_cast<uint32_t>(cause), false,
                                                  __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
}

uint32_t LossLatch::word() const
{
    AV_SCOPE("LossLatch::word");
    return __atomic_load_n(&word_, __ATOMIC_ACQUIRE);
}

bool LossLatch::lost() const
{
    AV_SCOPE("LossLatch::lost");
    return word() != 0u;
}

FaultCode LossLatch::cause() const
{
    AV_SCOPE("LossLatch::cause");
    return static_cast<FaultCode>(word() & ~lost_bit);
}

bool LossLatch::guilty() const
{
    AV_SCOPE("LossLatch::guilty");
    const uint32_t code = static_cast<uint32_t>(cause());
    return code >= static_cast<uint32_t>(FaultCode::ring_header) &&
           code <= static_cast<uint32_t>(FaultCode::undecodable);
}

/* Keyed by the entry and the cause: an application keeps calling a lost
 * context, and every call after the first says nothing new. */
bool LossLatch::refuses(const char *entry)
{
    AV_SCOPE("LossLatch::refuses");
    if (!lost()) return false;
    const uint32_t code = static_cast<uint32_t>(cause());
    if (lines_.first(entry, code))
        LogLine(lines_).text(entry).text(": the context is lost, fault code ").number(code).write();
    return true;
}

void LossLatch::forget()
{
    AV_SCOPE("LossLatch::forget");
    __atomic_store_n(&word_, 0u, __ATOMIC_RELEASE);
}

void LossLatch::report(StateWriter &writer) const
{
    AV_SCOPE("LossLatch::report");
    writer.flag("lost", lost());
    writer.number("fault_code", static_cast<uint32_t>(cause()));
}

bool LossLatch::invariants_hold() const
{
    AV_SCOPE("LossLatch::invariants_hold");
    const uint32_t now = word();
    return now == 0u || (now & lost_bit) != 0u;
}

}  // namespace av::icd
