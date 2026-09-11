/* The result of every fallible operation in core. Chosen so the shim maps each
 * value 1:1 onto an NTSTATUS without core knowing what an NTSTATUS is. */
#ifndef AV_STATUS_HPP
#define AV_STATUS_HPP

namespace av {

enum class Status : unsigned {
    ok = 0,
    invalid_parameter,
    no_memory,
    not_found,
    conflict,
    no_space,
    not_supported,
    invalid_state,
    too_large,
    /* Bytes no valid peer can produce: short, bad magic, non-zero reserved,
     * a field out of range. */
    malformed
};

}  // namespace av

#endif  // AV_STATUS_HPP
