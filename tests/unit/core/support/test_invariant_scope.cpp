#include "core/support/invariant_scope.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

using av::InvariantScope;
using av::MockPlatform;
using av::Violation;

namespace {

class Subject {
public:
    bool invariants_hold() const { return consistent_; }
    void set_consistent(bool consistent) { consistent_ = consistent; }

private:
    bool consistent_ = true;
};

}  // namespace

TEST(invariant_scope, is_silent_while_the_invariants_hold)
{ MockPlatform platform; Subject subject;
  { InvariantScope<Subject> guard(platform, subject, "probe"); }
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(invariant_scope, reports_an_object_broken_on_entry_and_still_broken_on_exit)
{ MockPlatform platform; Subject subject;
  subject.set_consistent(false);
  { InvariantScope<Subject> guard(platform, subject, "probe"); }
  CHECK(platform.claim(Violation::broken_invariant));
  CHECK(platform.claim(Violation::broken_invariant)); }

TEST(invariant_scope, reports_an_object_the_scope_left_broken)
{ MockPlatform platform; Subject subject;
  { InvariantScope<Subject> guard(platform, subject, "probe");
    subject.set_consistent(false); }
  CHECK(platform.claim(Violation::broken_invariant));
  CHECK(!platform.claim(Violation::broken_invariant)); }
