#include "executor/sharing/share_holder.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"

using av::MockSessionBackend;
using av::ShareHolder;

TEST(share_holder, names_its_session_and_its_handle)
{ MockSessionBackend session;
  const ShareHolder holder(session, 0x9001u, 0x77u);
  CHECK(&holder.session() == &session);
  CHECK_EQ(holder.handle(), 0x9001u);
  CHECK_EQ(holder.detail(), 0x77u);
  CHECK(holder.next() == nullptr); }

TEST(share_holder, links_to_the_next_holder)
{ MockSessionBackend first_session;
  MockSessionBackend second_session;
  ShareHolder first(first_session, 1u, 0u);
  ShareHolder second(second_session, 2u, 0u);
  first.link(&second);
  CHECK(first.next() == &second);
  first.link(nullptr);
  CHECK(first.next() == nullptr); }
