#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_context_group.hpp"
#include "icd/opencl/cl_device.hpp"
#include "icd/opencl/guest_cl_context.hpp"

#include <cstdint>

using av::clicd::ClContextGroup;
using av::clicd::ClDevice;
using av::clicd::ClObject;
using av::clicd::GuestClContext;

namespace {

GuestClContext *session(std::uintptr_t number) { return reinterpret_cast<GuestClContext *>(number * 0x100u); }
ClDevice       *device(std::uintptr_t number) { return reinterpret_cast<ClDevice *>(number * 0x1000u); }

}

TEST(cl_context_group, a_new_group_has_no_member_and_names_no_session)
{ ClContextGroup group;
  CHECK_EQ(group.count(), 0u);
  CHECK(group.of(session(1u)) == nullptr);
  CHECK(group.of(device(1u)) == nullptr);
  CHECK(!group.has(session(1u)));
  CHECK(!group.invariants_hold() || group.count() == 0u); }

TEST(cl_context_group, a_member_is_found_by_its_session_and_by_its_device)
{ ClContextGroup group;
  ClObject        *owner = reinterpret_cast<ClObject *>(std::uintptr_t{0x7000u});
  group.bind(*owner);
  CHECK(group.add(*session(1u), *device(1u), 0x61u));
  CHECK(group.add(*session(2u), *device(2u), 0x62u));
  CHECK_EQ(group.count(), 2u);
  CHECK(&group.owner() == owner);
  CHECK(group.of(session(2u)) != nullptr);
  CHECK_EQ(group.of(session(2u))->host_context, 0x62u);
  CHECK(group.of(device(1u)) == group.of(session(1u)));
  CHECK(group.has(session(1u)));
  CHECK(!group.has(session(3u)));
  CHECK_EQ(group.at(1u).copy_queue, 0u);
  CHECK_EQ(group.at(1u).done_event, 0u);
  CHECK(group.invariants_hold()); }

TEST(cl_context_group, a_session_or_a_device_joins_once_and_a_member_needs_a_host_context)
{ ClContextGroup group;
  ClObject        *owner = reinterpret_cast<ClObject *>(std::uintptr_t{0x7000u});
  group.bind(*owner);
  CHECK(group.add(*session(1u), *device(1u), 0x61u));
  CHECK(!group.add(*session(1u), *device(2u), 0x62u));
  CHECK(!group.add(*session(2u), *device(1u), 0x62u));
  CHECK(!group.add(*session(2u), *device(2u), 0u));
  CHECK_EQ(group.count(), 1u);
  CHECK(group.invariants_hold()); }

TEST(cl_context_group, a_group_holds_one_member_per_adapter_and_no_more)
{ ClContextGroup group;
  ClObject        *owner = reinterpret_cast<ClObject *>(std::uintptr_t{0x7000u});
  group.bind(*owner);
  for (std::uintptr_t index = 1u; index <= ClContextGroup::most; ++index)
      CHECK(group.add(*session(index), *device(index), 0x60u + index));
  CHECK(!group.add(*session(ClContextGroup::most + 1u), *device(ClContextGroup::most + 1u), 0x99u));
  CHECK(group.invariants_hold()); }
