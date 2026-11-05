#include "harness/framework/test_registry.hpp"
#include "harness/icd/version_text_probe.hpp"
#include "icd/opencl/cl_platform.hpp"

#include <CL/cl_ext.h>

#include <cstring>

using av::clicd::ClPlatform;
using av::clicd::ClPlatformProbe;

namespace av::clicd {
struct ClPlatformProbe {
    static void a_version_past_what_is_implemented(ClPlatform &platform)
    { av::icd::VersionTextProbe::set(platform.version_number_, platform.version_number_.major() + 1u, 0u); }
    static void a_minor_past_what_is_implemented(ClPlatform &platform)
    { av::icd::VersionTextProbe::set(platform.version_number_, platform.version_number_.major(), platform.version_number_.minor() + 1u); }
    static void an_empty_version(ClPlatform &platform) { platform.version_[0] = '\0'; }
    static void an_unterminated_version(ClPlatform &platform)
    { for (char &byte : platform.version_) byte = 'X'; }
};

}

namespace {
bool reads_back(const ClPlatform &platform, cl_platform_info name, char *into, std::size_t room)
{
    std::size_t bytes = 0u;
    if (platform.info(nullptr, name, 0u, nullptr, &bytes) != CL_SUCCESS) return false;
    if (bytes == 0u || bytes > room) return false;
    if (platform.info(nullptr, name, bytes, into, nullptr) != CL_SUCCESS) return false;
    return std::strlen(into) + 1u == bytes;
}

}

TEST(cl_platform, answers_the_suffix_the_loader_cannot_do_without)
{ ClPlatform platform;
  char text[64] = {};
  CHECK(reads_back(platform, CL_PLATFORM_ICD_SUFFIX_KHR, text, sizeof text));
  CHECK_EQ(std::strcmp(text, "AV"), 0);
  CHECK(platform.invariants_hold()); }

TEST(cl_platform, answers_its_profile_name_and_vendor)
{ ClPlatform platform;
  char text[64] = {};
  CHECK(reads_back(platform, CL_PLATFORM_PROFILE, text, sizeof text));
  CHECK_EQ(std::strcmp(text, "FULL_PROFILE"), 0);
  CHECK(reads_back(platform, CL_PLATFORM_NAME, text, sizeof text));
  CHECK(std::strstr(text, "avgpu") != nullptr);
  CHECK(reads_back(platform, CL_PLATFORM_VENDOR, text, sizeof text));
  CHECK(std::strlen(text) > 0u); }

TEST(cl_platform, the_version_says_opencl_and_a_number_the_way_the_specification_fixes)
{ ClPlatform platform;
  char text[64] = {};
  CHECK(reads_back(platform, CL_PLATFORM_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL ", 7u), 0);
  CHECK(text[7] >= '0' && text[7] <= '9');
  CHECK_EQ(text[8], '.');
  CHECK(text[9] >= '0' && text[9] <= '9');
  CHECK_EQ(text[10], ' '); }

TEST(cl_platform, a_host_that_answers_more_than_this_icd_implements_is_capped)
{ ClPlatform platform;
  char text[64] = {};
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
  CHECK_EQ(text[0], 'z'); }

TEST(cl_platform, a_size_may_be_asked_for_with_no_buffer_at_all)
{ ClPlatform platform;
  std::size_t bytes = 0u;
  CHECK_EQ(platform.info(nullptr, CL_PLATFORM_NAME, 0u, nullptr, &bytes), CL_SUCCESS);
  CHECK(bytes > 1u);
  char text[64] = {};
  CHECK_EQ(platform.info(nullptr, CL_PLATFORM_NAME, sizeof text, text, nullptr), CL_SUCCESS);
  CHECK_EQ(std::strlen(text) + 1u, bytes); }

TEST(cl_platform, a_query_this_platform_does_not_carry_is_an_invalid_value)
{ ClPlatform platform;
  std::size_t bytes = 1u;
  CHECK_EQ(platform.info(nullptr, 0x9999u, 0u, nullptr, &bytes), CL_INVALID_VALUE);
  CHECK_EQ(bytes, 1u); }

TEST(cl_platform, another_platforms_handle_is_an_invalid_platform)
{ ClPlatform platform;
  ClPlatform other;
  char text[64] = {};
  CHECK_EQ(platform.info(other.as_handle(), CL_PLATFORM_NAME, sizeof text, text, nullptr),
           CL_INVALID_PLATFORM);
  CHECK_EQ(platform.info(platform.as_handle(), CL_PLATFORM_NAME, sizeof text, text, nullptr),
           CL_SUCCESS);
  CHECK(platform.names_this(nullptr));
  CHECK(!platform.names_this(other.as_handle())); }

TEST(cl_platform, the_handle_the_loader_holds_is_the_platform_itself)
{ ClPlatform platform;
  CHECK(reinterpret_cast<void *>(platform.as_handle()) == static_cast<void *>(&platform)); }

TEST(cl_platform, a_platform_the_loader_would_drop_is_one_with_no_dispatch)
{ ClPlatform platform;
  CHECK(!platform.dispatchable());
  cl_icd_dispatch table = {};
  platform.dispatch_through(table);
  CHECK(platform.dispatchable());
  CHECK(platform.invariants_hold()); }

TEST(cl_platform, a_query_this_driver_does_not_answer_is_an_invalid_value)
{ ClPlatform platform;
  std::size_t bytes = 0xEEu;
  CHECK_EQ(platform.info(nullptr, 0x9999u, 0u, nullptr, &bytes), CL_INVALID_VALUE);
  cl_version version = 0u;
  CHECK_EQ(platform.info(nullptr, CL_PLATFORM_NUMERIC_VERSION, sizeof version - 1u, &version,
                         nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(version, 0u);
  std::size_t room = 0u;
  CHECK_EQ(platform.info(nullptr, CL_PLATFORM_NUMERIC_VERSION, 0u, nullptr, &room), CL_SUCCESS);
  CHECK_EQ(room, sizeof(cl_version));
  version = 0u;
  CHECK_EQ(platform.info(nullptr, CL_PLATFORM_NUMERIC_VERSION, sizeof version, &version, nullptr),
           CL_SUCCESS);
  CHECK(version != 0u);
  CHECK(platform.invariants_hold()); }

TEST(cl_platform, the_invariant_catches_a_version_it_could_not_have)
{ const auto broken = [](void (*corrupt)(ClPlatform &)) {
      ClPlatform platform;
      CHECK(platform.invariants_hold());
      corrupt(platform);
      return platform.invariants_hold();
  };
  CHECK(!broken(ClPlatformProbe::a_version_past_what_is_implemented));
  CHECK(!broken(ClPlatformProbe::a_minor_past_what_is_implemented));
  CHECK(!broken(ClPlatformProbe::an_empty_version));
  CHECK(!broken(ClPlatformProbe::an_unterminated_version)); }

TEST(cl_platform, the_name_carries_the_adapter_s_once_it_is_known)
{ ClPlatform platform;
  char text[160] = {};
  CHECK(!platform.names_an_adapter());
  CHECK(reads_back(platform, CL_PLATFORM_NAME, text, sizeof text));
  CHECK_EQ(std::strcmp(text, "avgpu OpenCL"), 0);
  platform.observe_adapter("");
  platform.observe_adapter(nullptr);
  CHECK(!platform.names_an_adapter());
  platform.observe_adapter("Intel(R) Arc(TM) Graphics");
  CHECK(platform.names_an_adapter());
  CHECK(reads_back(platform, CL_PLATFORM_NAME, text, sizeof text));
  CHECK_EQ(std::strcmp(text, "avgpu OpenCL on Intel(R) Arc(TM) Graphics"), 0); }
