/* Why FakeDxgkrnl refused a DDI table - one value per check dxgkrnl applies at
 * registration (docs/ddi-sequences.md, section 1). */
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

}  // namespace av

#endif  // AV_REGISTRATION_RULE_HPP
