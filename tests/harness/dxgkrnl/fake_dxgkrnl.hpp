#ifndef AV_FAKE_DXGKRNL_HPP
#define AV_FAKE_DXGKRNL_HPP

#include "miniport/driver/ddi_slot.hpp"
#include "miniport/driver/ddi_table.hpp"
#include "core/support/status.hpp"
#include "harness/dxgkrnl/registration_rule.hpp"

#include <cstdint>

namespace av {

class FakeDxgkrnl {
public:
    FakeDxgkrnl();
    FakeDxgkrnl(const FakeDxgkrnl &) = delete;
    FakeDxgkrnl &operator=(const FakeDxgkrnl &) = delete;

    [[nodiscard]] Status initialize(const DdiTable *table);

    RegistrationRule failed_rule() const;
    DdiSlot          missing_slot() const;
    const DdiTable  *registered_table() const;

private:
    static constexpr std::uint32_t oldest_accepted_version = 0x104Eu;

    Status refuse(RegistrationRule rule, Status status);
    bool   has_required_slots(const DdiTable &table);

    const DdiTable  *table_;
    RegistrationRule failed_;
    DdiSlot          missing_;
};

}

#endif
