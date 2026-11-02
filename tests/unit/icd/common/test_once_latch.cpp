#include "harness/framework/test_registry.hpp"
#include "icd/common/once_latch.hpp"

using av::icd::OnceLatch;

TEST(once_latch, says_first_once_and_never_again)
{ OnceLatch latch;
  CHECK(latch.first());
  CHECK(!latch.first());
  CHECK(!latch.first());
  OnceLatch other;
  CHECK(other.first()); }
