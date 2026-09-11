/* The dxgkrnl side of the DDI contract, as recovered from dxgkrnl.sys
 * 10.0.26100.9444 (docs/ddi-sequences.md). Registration only for now: the
 * scenario runner that drives DDIs arrives with the first DDIs (feature 006).
 *
 * Status mapping from the NTSTATUS dxgkrnl returns: STATUS_REVISION_MISMATCH ->
 * not_supported, STATUS_OBJECT_NAME_COLLISION -> conflict. */
#ifndef AV_FAKE_DXGKRNL_HPP
#define AV_FAKE_DXGKRNL_HPP

#include "../../src/core/ddi_slot.hpp"
#include "../../src/core/ddi_table.hpp"
#include "../../src/core/status.hpp"
#include "registration_rule.hpp"

#include <cstdint>

namespace av {

class FakeDxgkrnl {
public:
    FakeDxgkrnl();
    FakeDxgkrnl(const FakeDxgkrnl &) = delete;
    FakeDxgkrnl &operator=(const FakeDxgkrnl &) = delete;

    /* DxgkInitialize: DpiInitializeInternal's checks, then DpiInitializeEx's
     * one-registration-per-driver-object rule. */
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

}  // namespace av

#endif  // AV_FAKE_DXGKRNL_HPP
