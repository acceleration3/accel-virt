/* The rules MockPlatform enforces. Each kind is recorded separately, so a test
 * claims exactly the rule it meant to break. */
#ifndef AV_VIOLATION_HPP
#define AV_VIOLATION_HPP

namespace av {

enum class Violation : unsigned {
    irql_ceiling,
    allocation_at_dispatch,
    blocking_at_dispatch,
    lock_order,
    release_order,
    lock_stack_overflow,
    null_release,
    broken_invariant,
    time_overflow,
    unmapped_register,
    count
};

}  // namespace av

#endif  // AV_VIOLATION_HPP
