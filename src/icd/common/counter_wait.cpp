#include "icd/common/counter_wait.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"

namespace av::icd {

CounterWait::CounterWait(const Kmt &kmt, uint64_t timeout)
    : kmt_(kmt), frequency_(0u), start_(kmt.performance_counter(frequency_)), wait_(timeout, start_, frequency_)
{
    AV_SCOPE("CounterWait::CounterWait");
}

bool CounterWait::next(uint64_t &slice)
{
    AV_SCOPE("CounterWait::next");
    return wait_.next(kmt_.performance_counter(frequency_), slice);
}

}  // namespace av::icd
