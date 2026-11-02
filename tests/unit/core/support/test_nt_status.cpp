#include "core/support/nt_status.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"

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
{ const NtStatus status(0xC01D0008u);
  CHECK_EQ(static_cast<std::uint32_t>(status.value()), 0xC01D0008u);
  CHECK(!status.succeeded());
  CHECK(NtStatus(Status::ok).succeeded()); }

TEST(nt_status, carries_a_callback_answer_as_it_came)
{ CHECK_EQ(static_cast<std::uint32_t>(NtStatus(static_cast<std::int32_t>(0xC01E0314u)).value()), 0xC01E0314u);
  CHECK(NtStatus(0x401E034C).succeeded());
  CHECK(NtStatus(0x401E034C).status() == Status::ok); }

TEST(nt_status, names_the_graphics_statuses_of_a_vidpn)
{ CHECK_EQ(NtStatus::source_not_in_topology, 0xC01E0339u);
  CHECK_EQ(NtStatus::mode_already_in_mode_set, 0xC01E0314u);
  CHECK_EQ(NtStatus::invalid_video_present_source_mode, 0xC01E0310u);
  CHECK_EQ(NtStatus::path_content_geometry_transformation_not_supported, 0xC01E0346u);
  CHECK_EQ(NtStatus::no_recommended_functional_vidpn, 0xC01E0323u);
  CHECK_EQ(NtStatus::no_recommended_vidpn_topology, 0xC01E031Au); }

TEST(nt_status, not_implemented_is_status_not_implemented)
{ CHECK_EQ(NtStatus::not_implemented, 0xC0000002u);
  CHECK(!NtStatus(NtStatus::not_implemented).succeeded()); }

TEST(nt_status_status, maps_a_callback_answer_back_to_the_core_status)
{ struct Case { std::uint32_t value; Status status; };
  const Case cases[] = {
      {0x00000000u, Status::ok},
      {0x00000103u, Status::ok},
      {0xC000000Du, Status::invalid_parameter},
      {0xC0000017u, Status::no_memory},
      {0xC0000225u, Status::not_found},
      {0xC0000035u, Status::conflict},
      {0xC000009Au, Status::no_space},
      {0xC00000BBu, Status::not_supported},
      {0xC0000184u, Status::invalid_state},
      {0xC0000023u, Status::too_large},
      {0xC0000001u, Status::invalid_state},
  };
  for (const Case &c : cases)
      CHECK_STATUS(NtStatus(c.value).status(), c.status); }

TEST(nt_status, an_out_of_range_status_is_an_internal_error)
{ CHECK_EQ(static_cast<std::uint32_t>(NtStatus(static_cast<Status>(200u)).value()), 0xC00000E5u); }
