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
    malformed
};

}

#endif
