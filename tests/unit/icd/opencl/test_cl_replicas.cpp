#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_replicas.hpp"

#include <cstdint>

using av::clicd::ClReplicas;
using av::clicd::GuestClContext;

namespace {

GuestClContext *session(std::uintptr_t number)
{
    return reinterpret_cast<GuestClContext *>(number * 0x100u);
}

}

TEST(cl_replicas, a_new_set_names_no_session)
{ ClReplicas replicas;
  CHECK_EQ(replicas.count(), 0u);
  CHECK_EQ(replicas.in(session(1u)), 0u);
  CHECK(replicas.invariants_hold()); }

TEST(cl_replicas, each_session_holds_the_one_handle_it_was_given)
{ ClReplicas replicas;
  CHECK(replicas.add(*session(1u), 0x11u));
  CHECK(replicas.add(*session(2u), 0x22u));
  CHECK_EQ(replicas.count(), 2u);
  CHECK_EQ(replicas.in(session(1u)), 0x11u);
  CHECK_EQ(replicas.in(session(2u)), 0x22u);
  CHECK_EQ(replicas.in(session(3u)), 0u);
  CHECK(replicas.at(1u).session == session(2u));
  CHECK(replicas.invariants_hold()); }

TEST(cl_replicas, a_session_is_named_once_and_a_null_handle_is_no_replica)
{ ClReplicas replicas;
  CHECK(replicas.add(*session(1u), 0x11u));
  CHECK(!replicas.add(*session(1u), 0x12u));
  CHECK(!replicas.add(*session(2u), 0u));
  CHECK_EQ(replicas.count(), 1u);
  CHECK_EQ(replicas.in(session(1u)), 0x11u); }

TEST(cl_replicas, a_set_holds_one_replica_per_adapter_and_no_more)
{ ClReplicas replicas;
  for (std::uintptr_t index = 1u; index <= ClReplicas::most; ++index) CHECK(replicas.add(*session(index), 0x10u + index));
  CHECK_EQ(replicas.count(), ClReplicas::most);
  CHECK(!replicas.add(*session(ClReplicas::most + 1u), 0x99u));
  CHECK(replicas.invariants_hold()); }
