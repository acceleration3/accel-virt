#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/common/calling_thread.hpp"
#include "icd/d3d/reentrant_call_lock.hpp"

#include <cstdint>

using av::d3d::ReentrantCallLock;

namespace {

class Caller final : public av::icd::CallingThread {
public:
    std::uint32_t thread_id() override { return id_; }
    void          become(std::uint32_t id) { id_ = id; }

private:
    std::uint32_t id_ = 7u;
};

struct Rig {
    av::LockTrace           trace;
    av::RecordingThreadLock lock{trace, 'D'};
    Caller                  caller;
    ReentrantCallLock       reentrant{lock, caller};
};

}

TEST(reentrant_call_lock, a_new_lock_is_free_and_sound)
{ Rig rig;
  CHECK(rig.reentrant.invariants_hold());
  CHECK(!rig.lock.held());
  CHECK_EQ(rig.lock.acquisitions(), 0u); }

TEST(reentrant_call_lock, the_first_acquire_takes_the_lock_and_the_last_release_gives_it_back)
{ Rig rig;
  rig.reentrant.acquire();
  CHECK(rig.lock.held());
  CHECK_EQ(rig.lock.acquisitions(), 1u);
  CHECK(rig.reentrant.invariants_hold());
  rig.reentrant.release();
  CHECK(!rig.lock.held());
  CHECK(rig.reentrant.invariants_hold()); }

TEST(reentrant_call_lock, the_owner_may_acquire_again_without_taking_the_lock_twice_and_must_release_as_often)
{ Rig rig;
  rig.reentrant.acquire();
  rig.reentrant.acquire();
  rig.reentrant.acquire();
  CHECK_EQ(rig.lock.acquisitions(), 1u);
  CHECK(rig.reentrant.invariants_hold());
  rig.reentrant.release();
  rig.reentrant.release();
  CHECK(rig.lock.held());
  rig.reentrant.release();
  CHECK(!rig.lock.held());
  CHECK(rig.reentrant.invariants_hold()); }

TEST(reentrant_call_lock, a_released_lock_is_taken_afresh_by_whichever_thread_comes_next)
{ Rig rig;
  rig.reentrant.acquire();
  rig.reentrant.release();
  rig.caller.become(9u);
  rig.reentrant.acquire();
  CHECK_EQ(rig.lock.acquisitions(), 2u);
  CHECK(rig.lock.held());
  rig.reentrant.acquire();
  CHECK_EQ(rig.lock.acquisitions(), 2u);
  rig.reentrant.release();
  rig.reentrant.release();
  CHECK(!rig.lock.held());
  CHECK(rig.reentrant.invariants_hold()); }
