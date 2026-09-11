#include "../../src/core/nt_status.hpp"
#include "../../src/core/status.hpp"
#include "../harness/test_registry.hpp"

#include <cstdint>

using av::NtStatus;
using av::Status;

TEST(nt_status, maps_every_core_status_to_its_ntstatus)
{ struct Case { Status status; std::uint32_t value; };
  const Case cases[] = {
      {Status::ok, 0x00000000u},
      {Status::invalid_parameter, 0xC000000Du},
      {Status::no_memory, 0xC0000017u},
      {Status::not_found, 0xC0000225u},
      {Status::conflict, 0xC0000035u},
      {Status::no_space, 0xC000009Au},
      {Status::not_supported, 0xC00000BBu},
      {Status::invalid_state, 0xC0000184u},
      {Status::too_large, 0xC0000023u},
      {Status::malformed, 0xC000000Du},
  };
  for (const Case &c : cases)
      CHECK_EQ(static_cast<std::uint32_t>(NtStatus(c.status).value()), c.value); }

TEST(nt_status, carries_a_graphics_status_core_has_no_name_for)
{ const NtStatus status(NtStatus::monitor_no_more_descriptor_data);
  CHECK_EQ(static_cast<std::uint32_t>(status.value()), 0xC01D0003u);
  CHECK(!status.succeeded());
  CHECK(NtStatus(Status::ok).succeeded()); }

TEST(nt_status, an_out_of_range_status_is_an_internal_error)
{ CHECK_EQ(static_cast<std::uint32_t>(NtStatus(static_cast<Status>(200u)).value()), 0xC00000E5u); }
