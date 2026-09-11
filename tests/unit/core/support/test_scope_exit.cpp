#include "core/support/scope_exit.hpp"
#include "harness/framework/test_registry.hpp"

using av::ScopeExit;

namespace {

class Counter {
public:
    void bump() { count_++; }
    int  count() const { return count_; }

private:
    int count_ = 0;
};

int leave_early(Counter &counter, bool early)
{
    ScopeExit cleanup([&counter] { counter.bump(); });
    if (early) return 1;
    return 2;
}

}  // namespace

TEST(scope_exit, runs_when_the_scope_ends)
{ Counter counter;
  { ScopeExit cleanup([&counter] { counter.bump(); }); }
  CHECK_EQ(counter.count(), 1); }

TEST(scope_exit, runs_on_every_return_path)
{ Counter counter;
  CHECK_EQ(leave_early(counter, true), 1);
  CHECK_EQ(leave_early(counter, false), 2);
  CHECK_EQ(counter.count(), 2); }

TEST(scope_exit, dismiss_cancels_the_action)
{ Counter counter;
  { ScopeExit cleanup([&counter] { counter.bump(); });
    cleanup.dismiss(); }
  CHECK_EQ(counter.count(), 0); }
