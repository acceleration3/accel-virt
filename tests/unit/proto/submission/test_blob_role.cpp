#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/blob_role.hpp"
#include "proto/wire/guest_bytes.hpp"

#include <cstdint>

using av::BlobRole;
using av::GuestBytes;
using av::Status;

TEST(blob_role, a_default_role_is_plain_with_no_base)
{ const BlobRole role;
  CHECK(role.kind() == BlobRole::Kind::plain);
  CHECK_EQ(role.base(), 0u);
  CHECK(!role.is_arena());
  CHECK(role.invariants_hold()); }

TEST(blob_role, an_arena_role_is_valid_at_every_slot_and_no_other_offset)
{ for (std::uint32_t slot = 0u; slot < GuestBytes::slot_count; slot++) {
      const BlobRole role(BlobRole::Kind::arena_segment, std::uint64_t{slot} << GuestBytes::slot_shift);
      CHECK(role.is_arena());
      CHECK(role.invariants_hold());
  }
  CHECK(!BlobRole(BlobRole::Kind::arena_segment, 1u).invariants_hold());
  CHECK(!BlobRole(BlobRole::Kind::arena_segment, GuestBytes::slot_bytes + 1u).invariants_hold());
  CHECK(!BlobRole(BlobRole::Kind::arena_segment, GuestBytes::slot_bytes * GuestBytes::slot_count).invariants_hold());
  CHECK(!BlobRole(BlobRole::Kind::arena_segment, ~std::uint64_t{0}).invariants_hold()); }

TEST(blob_role, a_kind_that_does_not_exist_breaks_the_invariant_at_any_base)
{ CHECK(!BlobRole(static_cast<BlobRole::Kind>(2u), 0u).invariants_hold());
  CHECK(!BlobRole(static_cast<BlobRole::Kind>(0xFFFFFFFFu), GuestBytes::slot_bytes).invariants_hold()); }

TEST(blob_role, a_plain_role_carries_no_base)
{ CHECK(!BlobRole(BlobRole::Kind::plain, GuestBytes::slot_bytes).invariants_hold());
  CHECK(!BlobRole(BlobRole::Kind::plain, 1u).invariants_hold()); }

TEST(blob_role_decode, reads_the_kinds_that_exist)
{ BlobRole role;
  CHECK_STATUS(role.decode(1u, GuestBytes::slot_bytes * 2u), Status::ok);
  CHECK(role == BlobRole(BlobRole::Kind::arena_segment, GuestBytes::slot_bytes * 2u));
  CHECK_STATUS(role.decode(0u, 0u), Status::ok);
  CHECK(role == BlobRole()); }

TEST(blob_role_decode, refuses_an_unknown_kind_or_a_bad_base_and_keeps_what_it_held)
{ BlobRole role(BlobRole::Kind::arena_segment, GuestBytes::slot_bytes);
  const BlobRole held = role;
  CHECK_STATUS(role.decode(2u, 0u), Status::invalid_parameter);
  CHECK_STATUS(role.decode(0xFFFFFFFFu, 0u), Status::invalid_parameter);
  CHECK_STATUS(role.decode(1u, 3u), Status::invalid_parameter);
  CHECK_STATUS(role.decode(0u, GuestBytes::slot_bytes), Status::invalid_parameter);
  CHECK(role == held); }

TEST(blob_role, roles_compare_by_kind_and_base)
{ CHECK(BlobRole() == BlobRole(BlobRole::Kind::plain, 0u));
  CHECK(!(BlobRole() == BlobRole(BlobRole::Kind::arena_segment, 0u)));
  CHECK(!(BlobRole(BlobRole::Kind::arena_segment, 0u) == BlobRole(BlobRole::Kind::arena_segment, GuestBytes::slot_bytes))); }
