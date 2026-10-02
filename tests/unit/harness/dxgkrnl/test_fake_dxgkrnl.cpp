#include "miniport/driver/ddi_slot.hpp"
#include "miniport/driver/ddi_table.hpp"
#include "core/support/status.hpp"
#include "harness/dxgkrnl/ddi_table_probe.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl.hpp"
#include "harness/framework/test_registry.hpp"

using av::DdiSlot;
using av::DdiTable;
using av::DdiTableProbe;
using av::FakeDxgkrnl;
using av::RegistrationRule;
using av::Status;

namespace {

void routine() {}

/* The slots DpiInitializeInternal requires, in the order it tests them. The
 * child-status slot is required unless QueryConnectionChange stands in for it. */
const DdiSlot required[] = {
    DdiSlot::DxgkDdiAddDevice,         DdiSlot::DxgkDdiStartDevice,
    DdiSlot::DxgkDdiStopDevice,        DdiSlot::DxgkDdiRemoveDevice,
    DdiSlot::DxgkDdiDispatchIoRequest, DdiSlot::DxgkDdiInterruptRoutine,
    DdiSlot::DxgkDdiDpcRoutine,        DdiSlot::DxgkDdiQueryChildRelations,
    DdiSlot::DxgkDdiQueryChildStatus,  DdiSlot::DxgkDdiQueryDeviceDescriptor,
    DdiSlot::DxgkDdiSetPowerState,     DdiSlot::DxgkDdiResetDevice,
    DdiSlot::DxgkDdiUnload,
};

class Registrable {
public:
    Registrable()
    {
        for (const DdiSlot slot : required)
            if (table_.set(slot, routine) != Status::ok)
                av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "set failed");
    }
    DdiTable &table() { return table_; }

private:
    DdiTable table_;
};

}  // namespace

TEST(fake_dxgkrnl_initialize, accepts_a_table_with_every_required_slot)
{ Registrable registrable; FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(dxgkrnl.initialize(&registrable.table()), Status::ok);
  CHECK(dxgkrnl.failed_rule() == RegistrationRule::none);
  CHECK(dxgkrnl.registered_table() == &registrable.table()); }

TEST(fake_dxgkrnl_initialize, rejects_each_missing_required_slot_naming_it)
{ for (const DdiSlot slot : required) {
      Registrable registrable; FakeDxgkrnl dxgkrnl;
      CHECK_STATUS(registrable.table().set(slot, nullptr), Status::ok);
      if (slot == DdiSlot::DxgkDdiQueryChildStatus) continue;
      CHECK_STATUS(dxgkrnl.initialize(&registrable.table()), Status::not_supported);
      CHECK(dxgkrnl.failed_rule() == RegistrationRule::required_slot_missing);
      CHECK(dxgkrnl.missing_slot() == slot);
      CHECK(dxgkrnl.registered_table() == nullptr); } }

TEST(fake_dxgkrnl_initialize, query_connection_change_stands_in_for_query_child_status)
{ Registrable registrable; FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(registrable.table().set(DdiSlot::DxgkDdiQueryChildStatus, nullptr), Status::ok);
  CHECK_STATUS(registrable.table().set(DdiSlot::DxgkDdiQueryConnectionChange, routine), Status::ok);
  CHECK_STATUS(dxgkrnl.initialize(&registrable.table()), Status::ok); }

TEST(fake_dxgkrnl_initialize, without_either_child_status_slot_names_query_child_status)
{ Registrable registrable; FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(registrable.table().set(DdiSlot::DxgkDdiQueryChildStatus, nullptr), Status::ok);
  CHECK_STATUS(dxgkrnl.initialize(&registrable.table()), Status::not_supported);
  CHECK(dxgkrnl.missing_slot() == DdiSlot::DxgkDdiQueryChildStatus); }

TEST(fake_dxgkrnl_initialize, notify_acpi_event_is_not_required)
{ Registrable registrable; FakeDxgkrnl dxgkrnl;
  CHECK(registrable.table().routine(DdiSlot::DxgkDdiNotifyAcpiEvent) == nullptr);
  CHECK_STATUS(dxgkrnl.initialize(&registrable.table()), Status::ok); }

TEST(fake_dxgkrnl_initialize, rejects_a_version_below_0x104e)
{ Registrable registrable; FakeDxgkrnl dxgkrnl;
  DdiTableProbe(registrable.table()).set_version(0x104Du);
  CHECK_STATUS(dxgkrnl.initialize(&registrable.table()), Status::not_supported);
  CHECK(dxgkrnl.failed_rule() == RegistrationRule::version_too_old);
  CHECK(dxgkrnl.registered_table() == nullptr); }

TEST(fake_dxgkrnl_initialize, the_lowest_accepted_version_is_0x104e)
{ Registrable registrable; FakeDxgkrnl dxgkrnl;
  DdiTableProbe(registrable.table()).set_version(0x104Eu);
  CHECK_STATUS(dxgkrnl.initialize(&registrable.table()), Status::ok); }

TEST(fake_dxgkrnl_initialize, checks_the_version_before_the_slots)
{ DdiTable empty; FakeDxgkrnl dxgkrnl;
  DdiTableProbe(empty).set_version(0x1000u);
  CHECK_STATUS(dxgkrnl.initialize(&empty), Status::not_supported);
  CHECK(dxgkrnl.failed_rule() == RegistrationRule::version_too_old); }

TEST(fake_dxgkrnl_initialize, a_second_registration_collides)
{ Registrable first; Registrable second; FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(dxgkrnl.initialize(&first.table()), Status::ok);
  CHECK_STATUS(dxgkrnl.initialize(&second.table()), Status::conflict);
  CHECK(dxgkrnl.failed_rule() == RegistrationRule::already_registered);
  CHECK(dxgkrnl.registered_table() == &first.table()); }

TEST(fake_dxgkrnl_initialize, a_null_table_is_rejected_without_dereference)
{ FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(dxgkrnl.initialize(nullptr), Status::invalid_parameter);
  CHECK(dxgkrnl.failed_rule() == RegistrationRule::no_table); }

TEST(fake_dxgkrnl_initialize, a_rejected_table_can_be_fixed_and_registered)
{ Registrable registrable; FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(registrable.table().set(DdiSlot::DxgkDdiUnload, nullptr), Status::ok);
  CHECK_STATUS(dxgkrnl.initialize(&registrable.table()), Status::not_supported);
  CHECK_STATUS(registrable.table().set(DdiSlot::DxgkDdiUnload, routine), Status::ok);
  CHECK_STATUS(dxgkrnl.initialize(&registrable.table()), Status::ok);
  CHECK(dxgkrnl.failed_rule() == RegistrationRule::none); }
