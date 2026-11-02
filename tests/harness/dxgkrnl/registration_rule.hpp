#ifndef AV_REGISTRATION_RULE_HPP
#define AV_REGISTRATION_RULE_HPP

namespace av {

enum class RegistrationRule : unsigned {
    none,
    no_table,
    version_too_old,
    required_slot_missing,
    already_registered,
};

}

#endif
