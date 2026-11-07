#include "harness/framework/test_registry.hpp"
#include "icd/d3d/session_refusal.hpp"

using av::d3d::SessionRefusal;

TEST(session_refusal, a_host_out_of_sessions_is_out_of_memory_to_direct3d_10_through_12)
{ const SessionRefusal exhausted(true);
  CHECK_EQ(exhausted.for_d3d(), av::d3d::hr_out_of_memory);
  CHECK_EQ(exhausted.for_d3d(), static_cast<av::d3d::Hresult>(0x8007000Eu)); }

TEST(session_refusal, a_host_out_of_sessions_is_out_of_video_memory_to_direct3d_9)
{ const SessionRefusal exhausted(true);
  CHECK_EQ(exhausted.for_d3d9(), av::d3d::hr_d3d9_out_of_video_memory);
  CHECK_EQ(exhausted.for_d3d9(), static_cast<av::d3d::Hresult>(0x8876017Cu)); }

TEST(session_refusal, any_other_refusal_stays_a_plain_failure)
{ const SessionRefusal other(false);
  CHECK_EQ(other.for_d3d(), av::d3d::hr_fail);
  CHECK_EQ(other.for_d3d9(), av::d3d::hr_fail); }
