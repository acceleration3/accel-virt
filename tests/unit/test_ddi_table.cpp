#include "../../src/core/ddi_slot.hpp"
#include "../../src/core/ddi_table.hpp"
#include "../../src/core/status.hpp"
#include "../harness/ddi_table_probe.hpp"
#include "../harness/test_registry.hpp"

#include <cstdint>

using av::DdiRoutine;
using av::DdiSlot;
using av::DdiTable;
using av::Status;

using av::DdiTableProbe;

namespace {

void first_routine() {}
void second_routine() {}

bool only_slot_set(const DdiTable &table, DdiSlot slot, DdiRoutine routine)
{
    for (std::uint16_t index = 1u; index < DdiTable::slot_count; index++) {
        const DdiRoutine expected = (index == static_cast<std::uint16_t>(slot)) ? routine : nullptr;
        if (table.routine(DdiSlot{index}) != expected) return false;
    }
    return true;
}

}  // namespace

TEST(ddi_table, starts_at_wddm_3_2_with_no_routines)
{ const DdiTable table;
  CHECK_EQ(table.version(), 0x11007u);
  CHECK(only_slot_set(table, DdiSlot::DxgkDdiAddDevice, nullptr));
  CHECK(table.invariants_hold()); }

TEST(ddi_table, has_one_slot_per_driver_initialization_data_field)
{ CHECK_EQ(DdiTable::slot_count, 193u);
  CHECK_EQ(static_cast<unsigned>(DdiSlot::Reserved), 69u);
  CHECK_EQ(static_cast<unsigned>(DdiSlot::DxgkDdiResetDisplayEngine), 192u); }

TEST(ddi_table_set, stores_a_routine_in_exactly_its_slot)
{ DdiTable table;
  CHECK_STATUS(table.set(DdiSlot::DxgkDdiStartDevice, first_routine), Status::ok);
  CHECK(table.invariants_hold());
  CHECK(only_slot_set(table, DdiSlot::DxgkDdiStartDevice, first_routine)); }

TEST(ddi_table_set, the_first_and_last_routine_slots_are_usable)
{ DdiTable table;
  CHECK_STATUS(table.set(DdiSlot::DxgkDdiAddDevice, first_routine), Status::ok);
  CHECK_STATUS(table.set(DdiSlot::DxgkDdiResetDisplayEngine, second_routine), Status::ok);
  CHECK(table.invariants_hold());
  CHECK(table.routine(DdiSlot::DxgkDdiAddDevice) == first_routine);
  CHECK(table.routine(DdiSlot::DxgkDdiResetDisplayEngine) == second_routine); }

TEST(ddi_table_set, replacing_and_clearing_a_routine)
{ DdiTable table;
  CHECK_STATUS(table.set(DdiSlot::DxgkDdiStopDevice, first_routine), Status::ok);
  CHECK_STATUS(table.set(DdiSlot::DxgkDdiStopDevice, second_routine), Status::ok);
  CHECK(table.routine(DdiSlot::DxgkDdiStopDevice) == second_routine);
  CHECK_STATUS(table.set(DdiSlot::DxgkDdiStopDevice, nullptr), Status::ok);
  CHECK(table.invariants_hold());
  CHECK(only_slot_set(table, DdiSlot::DxgkDdiStopDevice, nullptr)); }

TEST(ddi_table_set, rejects_the_version_slot)
{ DdiTable table;
  CHECK_STATUS(table.set(DdiSlot::Version, first_routine), Status::invalid_parameter);
  CHECK_EQ(table.version(), 0x11007u);
  CHECK(table.invariants_hold()); }

TEST(ddi_table_set, rejects_a_slot_past_the_end)
{ DdiTable table;
  CHECK_STATUS(table.set(DdiSlot{193}, first_routine), Status::invalid_parameter);
  CHECK_STATUS(table.set(DdiSlot{0xFFFF}, first_routine), Status::invalid_parameter);
  CHECK(table.invariants_hold());
  CHECK(only_slot_set(table, DdiSlot::DxgkDdiAddDevice, nullptr)); }

TEST(ddi_table_routine, reads_null_for_the_version_slot_and_past_the_end)
{ DdiTable table;
  CHECK(table.routine(DdiSlot::Version) == nullptr);
  CHECK(table.routine(DdiSlot{193}) == nullptr); }

TEST(ddi_table_invariants, a_corrupted_version_is_detected)
{ DdiTable table; DdiTableProbe probe(table);
  probe.set_version(0x7002u);
  CHECK(!table.invariants_hold());
  probe.set_version(0x11007u);
  CHECK(table.invariants_hold()); }

TEST(ddi_table_invariants, non_zero_padding_is_detected)
{ DdiTable table; DdiTableProbe probe(table);
  probe.set_padding(1u);
  CHECK(!table.invariants_hold()); }
