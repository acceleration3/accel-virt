#include "harness/framework/test_registry.hpp"
#include "proto/context/protocol_version.hpp"

using av::ProtocolVersion;

TEST(protocol_version, the_current_version_is_1_1)
{ CHECK_EQ(ProtocolVersion::current_major, 1u);
  CHECK_EQ(ProtocolVersion::current_minor, 1u);
  const ProtocolVersion version(2u, 7u);
  CHECK_EQ(version.major_version(), 2u);
  CHECK_EQ(version.minor_version(), 7u); }

TEST(protocol_version_compatible_with, requires_equal_majors)
{ CHECK(ProtocolVersion(1u, 0u).compatible_with(ProtocolVersion(1u, 9u)));
  CHECK(!ProtocolVersion(1u, 0u).compatible_with(ProtocolVersion(2u, 0u)));
  CHECK(!ProtocolVersion(2u, 0u).compatible_with(ProtocolVersion(1u, 0u))); }

TEST(protocol_version_negotiated_with, takes_the_smaller_minor)
{ const ProtocolVersion lower = ProtocolVersion(1u, 2u).negotiated_with(ProtocolVersion(1u, 5u));
  CHECK_EQ(lower.major_version(), 1u);
  CHECK_EQ(lower.minor_version(), 2u);
  const ProtocolVersion other = ProtocolVersion(1u, 5u).negotiated_with(ProtocolVersion(1u, 2u));
  CHECK_EQ(other.minor_version(), 2u); }
