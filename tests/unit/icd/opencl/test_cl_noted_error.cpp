/* A creation call's error (ClNotedError): the root writes it into the note's
 * own slot, the application's pointer - if it gave one - is handed it as the
 * note goes, and a failure is logged under the entry point's name either way,
 * which a creation with no error pointer would otherwise lose. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/cl_noted_error.hpp"

#include <CL/cl.h>
#include <cstdint>

using av::ClIcdRig;
using av::clicd::ClNotedError;

TEST(cl_noted_error, a_failure_reaches_the_application_and_the_log)
{ ClIcdRig rig;
  cl_int application = 99;
  { ClNotedError noted(rig.root(), "clCreateBuffer", &application);
    *noted.pointer() = CL_INVALID_VALUE; }
  CHECK_EQ(application, CL_INVALID_VALUE);
  CHECK_EQ(rig.log().holding("clCreateBuffer answered -30"), 1u); }

TEST(cl_noted_error, a_failure_with_nowhere_to_go_is_still_logged)
{ ClIcdRig rig;
  { ClNotedError noted(rig.root(), "clCreateContext", nullptr);
    *noted.pointer() = CL_OUT_OF_HOST_MEMORY; }
  CHECK_EQ(rig.log().holding("clCreateContext answered -6"), 1u); }

TEST(cl_noted_error, a_success_is_handed_back_and_logs_nothing)
{ ClIcdRig rig;
  cl_int application = 99;
  const std::uint32_t before = rig.log().lines();
  { ClNotedError noted(rig.root(), "clCreateCommandQueue", &application); }
  CHECK_EQ(application, CL_SUCCESS);
  CHECK_EQ(rig.log().lines(), before); }
