/* The platform the Khronos loader decides whether to keep.
 *
 * Two of these queries are load-bearing in a way the rest are not. The loader
 * asks CL_PLATFORM_EXTENSIONS and CL_PLATFORM_ICD_SUFFIX_KHR while it is
 * enumerating drivers, in every process that ever calls clGetPlatformIDs, and
 * a platform whose suffix it cannot read is dropped without a word. So both
 * are answered from the platform alone, with nothing opened.
 *
 * The rest is the API's own two-call shape - ask the size, then ask again with
 * room - and what it does when a caller gets that wrong. */
#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_platform.hpp"

#include <CL/cl_ext.h>

#include <cstring>

using av::clicd::ClPlatform;

namespace {

/* What the loader does: ask for the size, then for the bytes. */
bool reads_back(const ClPlatform &platform, cl_platform_info name, char *into, std::size_t room)
{
    std::size_t bytes = 0u;
    if (platform.info(nullptr, name, 0u, nullptr, &bytes) != CL_SUCCESS) return false;
    if (bytes == 0u || bytes > room) return false;
    if (platform.info(nullptr, name, bytes, into, nullptr) != CL_SUCCESS) return false;
    return std::strlen(into) + 1u == bytes;
}

}  // namespace

TEST(cl_platform, answers_the_suffix_the_loader_cannot_do_without)
{ ClPlatform platform;
  char text[64] = {};
  CHECK(reads_back(platform, CL_PLATFORM_ICD_SUFFIX_KHR, text, sizeof text));
  CHECK_EQ(std::strcmp(text, "RVK"), 0);
  CHECK(platform.invariants_hold()); }

TEST(cl_platform, answers_its_profile_name_and_vendor)
{ ClPlatform platform;
  char text[64] = {};
  CHECK(reads_back(platform, CL_PLATFORM_PROFILE, text, sizeof text));
  CHECK_EQ(std::strcmp(text, "FULL_PROFILE"), 0);
  CHECK(reads_back(platform, CL_PLATFORM_NAME, text, sizeof text));
  CHECK(std::strstr(text, "rvkgpu") != nullptr);
  CHECK(reads_back(platform, CL_PLATFORM_VENDOR, text, sizeof text));
  CHECK(std::strlen(text) > 0u); }

TEST(cl_platform, the_version_says_opencl_and_a_number_the_way_the_specification_fixes)
{ ClPlatform platform;
  char text[64] = {};
  CHECK(reads_back(platform, CL_PLATFORM_VERSION, text, sizeof text));
  /* "OpenCL <major>.<minor> <anything>" - a runtime that parses this is
   * entitled to the first three tokens. */
  CHECK_EQ(std::strncmp(text, "OpenCL ", 7u), 0);
  CHECK(text[7] >= '0' && text[7] <= '9');
  CHECK_EQ(text[8], '.');
  CHECK(text[9] >= '0' && text[9] <= '9');
  CHECK_EQ(text[10], ' '); }

TEST(cl_platform, a_host_that_answers_more_than_this_icd_implements_is_capped)
{ ClPlatform platform;
  char text[64] = {};
  /* The host reports OpenCL 3.5; what is reported is what is tested. */
  platform.cap_version(3u, 5u);
  CHECK(reads_back(platform, CL_PLATFORM_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 3.0", 10u), 0);
  platform.cap_version(9u, 9u);
  CHECK(reads_back(platform, CL_PLATFORM_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 3.0", 10u), 0);
  CHECK(platform.invariants_hold()); }

TEST(cl_platform, a_host_older_than_this_icd_is_reported_as_it_is)
{ ClPlatform platform;
  char text[64] = {};
  platform.cap_version(1u, 2u);
  CHECK(reads_back(platform, CL_PLATFORM_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 1.2", 10u), 0);
  CHECK(platform.invariants_hold()); }

TEST(cl_platform, the_numeric_version_agrees_with_the_string)
{ ClPlatform platform;
  platform.cap_version(1u, 2u);
  cl_version numeric = 0u;
  std::size_t bytes = 0u;
  CHECK_EQ(platform.info(nullptr, CL_PLATFORM_NUMERIC_VERSION, sizeof numeric, &numeric, &bytes),
           CL_SUCCESS);
  CHECK_EQ(bytes, sizeof(cl_version));
  CHECK_EQ(CL_VERSION_MAJOR(numeric), 1u);
  CHECK_EQ(CL_VERSION_MINOR(numeric), 2u); }

TEST(cl_platform, a_buffer_too_small_is_refused_rather_than_filled_as_far_as_it_goes)
{ ClPlatform platform;
  char text[4] = {'z', 'z', 'z', 'z'};
  CHECK_EQ(platform.info(nullptr, CL_PLATFORM_PROFILE, sizeof text, text, nullptr),
           CL_INVALID_VALUE);
  /* And nothing was written: a caller that shrank its buffer is told, not
   * handed a string cut where it happened to fit. */
  CHECK_EQ(text[0], 'z'); }

TEST(cl_platform, a_size_may_be_asked_for_with_no_buffer_at_all)
{ ClPlatform platform;
  std::size_t bytes = 0u;
  CHECK_EQ(platform.info(nullptr, CL_PLATFORM_NAME, 0u, nullptr, &bytes), CL_SUCCESS);
  CHECK(bytes > 1u);
  /* A buffer with no size ret is the other half of the same call. */
  char text[64] = {};
  CHECK_EQ(platform.info(nullptr, CL_PLATFORM_NAME, sizeof text, text, nullptr), CL_SUCCESS);
  CHECK_EQ(std::strlen(text) + 1u, bytes); }

TEST(cl_platform, a_query_this_platform_does_not_carry_is_an_invalid_value)
{ ClPlatform platform;
  std::size_t bytes = 1u;
  CHECK_EQ(platform.info(nullptr, 0x9999u, 0u, nullptr, &bytes), CL_INVALID_VALUE);
  /* And nothing is reported about a value it did not answer. */
  CHECK_EQ(bytes, 1u); }

TEST(cl_platform, another_platforms_handle_is_an_invalid_platform)
{ ClPlatform platform;
  ClPlatform other;
  char text[64] = {};
  CHECK_EQ(platform.info(other.as_handle(), CL_PLATFORM_NAME, sizeof text, text, nullptr),
           CL_INVALID_PLATFORM);
  /* Its own is not, and neither is null: the API lets a caller leave the
   * platform out where there is one. */
  CHECK_EQ(platform.info(platform.as_handle(), CL_PLATFORM_NAME, sizeof text, text, nullptr),
           CL_SUCCESS);
  CHECK(platform.names_this(nullptr));
  CHECK(!platform.names_this(other.as_handle())); }

TEST(cl_platform, the_handle_the_loader_holds_is_the_platform_itself)
{ ClPlatform platform;
  /* The ICD ABI is the first word of the object, so the address it hands out
   * has to be the object's own. */
  CHECK(reinterpret_cast<void *>(platform.as_handle()) == static_cast<void *>(&platform)); }

TEST(cl_platform, a_platform_the_loader_would_drop_is_one_with_no_dispatch)
{ ClPlatform platform;
  /* The loader skips a platform whose first word is null before it asks it
   * anything at all. */
  CHECK(!platform.dispatchable());
  cl_icd_dispatch table = {};
  platform.dispatch_through(table);
  CHECK(platform.dispatchable());
  CHECK(platform.invariants_hold()); }
