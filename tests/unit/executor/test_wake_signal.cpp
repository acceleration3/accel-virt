/* The signal that wakes a context's worker (WakeSignal): raised with the
 * context it was bound with, raising nothing while unbound or once cleared,
 * and bound again by the next session. */
#include "executor/wake_signal.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::WakeSignal;

namespace {

struct Pad {
    std::uint32_t raised = 0u;
};

void raise_pad(void *context) { static_cast<Pad *>(context)->raised++; }

}  // namespace

TEST(wake_signal, raises_the_context_it_was_bound_with)
{ WakeSignal wake;
  Pad        pad;
  CHECK(!wake.bound());
  CHECK(!wake.raise());
  wake.bind(&raise_pad, &pad);
  CHECK(wake.bound());
  CHECK(wake.raise());
  CHECK(wake.raise());
  CHECK_EQ(pad.raised, 2u); }

/* A session that ended raises nothing, and the next binds its own: a host
 * thread still running for the old one cannot wake the new context. */
TEST(wake_signal, a_cleared_signal_raises_nothing_until_the_next_session_binds)
{ WakeSignal wake;
  Pad        old_session, new_session;
  wake.bind(&raise_pad, &old_session);
  wake.clear();
  CHECK(!wake.bound());
  CHECK(!wake.raise());
  wake.bind(&raise_pad, &new_session);
  CHECK(wake.raise());
  CHECK_EQ(old_session.raised, 0u);
  CHECK_EQ(new_session.raised, 1u); }
