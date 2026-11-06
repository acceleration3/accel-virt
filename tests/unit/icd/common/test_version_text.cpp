#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "icd/common/version_text.hpp"

#include <cstdint>
#include <cstring>

using av::Status;
using av::icd::VersionText;

TEST(version_text, reads_the_version_after_its_prefix_and_leaves_the_rest)
{ VersionText version("OpenGL ", 4u, 6u);
  CHECK_EQ(version.major(), 4u);
  CHECK_EQ(version.minor(), 6u);
  const char  host[] = "OpenGL 4.5.0 NVIDIA 550";
  const char *rest = nullptr;
  CHECK_STATUS(version.read(host, sizeof host, rest), Status::ok);
  CHECK_EQ(version.major(), 4u);
  CHECK_EQ(version.minor(), 5u);
  CHECK_EQ(std::strcmp(rest, ".0 NVIDIA 550"), 0);
  CHECK(version.invariants_hold()); }

TEST(version_text, a_version_past_what_is_implemented_is_capped_to_it)
{ VersionText version("OpenGL ", 4u, 6u);
  const char *rest = nullptr;
  const char  newer[] = "OpenGL 4.7 x";
  CHECK_STATUS(version.read(newer, sizeof newer, rest), Status::ok);
  CHECK_EQ(version.major(), 4u);
  CHECK_EQ(version.minor(), 6u);
  const char  far[] = "OpenGL 9.0 x";
  CHECK_STATUS(version.read(far, sizeof far, rest), Status::ok);
  CHECK_EQ(version.major(), 4u);
  CHECK_EQ(version.minor(), 6u);
  const char  older[] = "OpenGL 3.3";
  CHECK_STATUS(version.read(older, sizeof older, rest), Status::ok);
  CHECK_EQ(version.major(), 3u);
  CHECK_EQ(version.minor(), 3u);
  CHECK_EQ(std::strlen(rest), 0u);
  CHECK(version.invariants_hold()); }

TEST(version_text, text_that_is_no_version_is_malformed_and_changes_nothing)
{ VersionText version("OpenGL ", 4u, 6u);
  version.cap(3u, 1u);
  const char *rest = nullptr;
  const char  cases[][16] = {"OpenGL x.y", "OpenGL 4", "OpenGL 4.", "OpenGL .5", "OpenGL 1234.1", "OpenGL 4.1234",
                             "Vulkan 1.0", "OpenGL"};
  for (const auto &text : cases) CHECK_STATUS(version.read(text, sizeof text, rest), Status::malformed);
  CHECK_STATUS(version.read(nullptr, 8u, rest), Status::malformed);
  const char unterminated[4] = {'O', 'p', 'e', 'n'};
  CHECK_STATUS(version.read(unterminated, sizeof unterminated, rest), Status::malformed);
  CHECK_EQ(version.major(), 3u);
  CHECK_EQ(version.minor(), 1u); }

TEST(version_text, writes_its_prefix_the_version_it_holds_and_a_tail)
{ VersionText version("OpenGL ", 4u, 6u);
  char          into[64] = {};
  version.write(into, sizeof into, " av host");
  CHECK_EQ(std::strcmp(into, "OpenGL 4.6 av host"), 0);
  version.cap(3u, 2u);
  version.write(into, sizeof into, "");
  CHECK_EQ(std::strcmp(into, "OpenGL 3.2"), 0);
  version.cap(5u, 0u);
  CHECK_EQ(version.major(), 4u);
  CHECK_EQ(version.minor(), 6u); }
