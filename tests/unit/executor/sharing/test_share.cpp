#include "executor/owned_fd.hpp"
#include "executor/sharing/share.hpp"
#include "executor/sharing/share_holder.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

using av::MockSessionBackend;
using av::OwnedFd;
using av::Share;
using av::ShareHolder;
using av::ShareOrigin;
using av::SharedImageShape;

TEST(share, names_its_object)
{ const Share share(3u, 0x2Au, 7u, 4096u, ShareOrigin::d3d12);
  CHECK_EQ(share.domain(), 3u);
  CHECK_EQ(share.id(), 0x2Au);
  CHECK_EQ(share.kind(), 7u);
  CHECK_EQ(share.size(), 4096u);
  CHECK(share.origin() == ShareOrigin::d3d12);
  CHECK_EQ(share.descriptor(), -1);
  CHECK_EQ(share.handle_kind(), 0u);
  CHECK(share.holders() == nullptr);
  CHECK(share.publisher() == nullptr);
  CHECK(!share.shape().described());
  CHECK(share.direct3d().resource() == nullptr); }

TEST(share, accepts_only_its_own_kind_and_size)
{ const Share share(1u, 1u, 7u, 4096u, ShareOrigin::object);
  CHECK(share.accepts(7u, 4096u));
  CHECK(!share.accepts(8u, 4096u));
  CHECK(!share.accepts(7u, 8192u));
  CHECK(!share.accepts(7u, 4095u)); }

TEST(share, a_direct3d_share_is_described_once_it_is_published_by_handle)
{ Share share(1u, 1u, 0u, 0u, ShareOrigin::d3d11);
  CHECK(!share.accepts(7u, 4096u));
  share.describe(7u, 4096u);
  CHECK(share.accepts(7u, 4096u)); }

TEST(share, the_guest_holds_it_until_its_driver_lets_go)
{ Share share(1u, 1u, 7u, 4096u, ShareOrigin::object);
  CHECK(share.guest_holds());
  CHECK(!share.unheld());
  share.let_go_by_guest();
  CHECK(!share.guest_holds());
  CHECK(share.unheld()); }

TEST(share, a_pinned_share_is_held)
{ Share share(1u, 1u, 7u, 4096u, ShareOrigin::object);
  share.let_go_by_guest();
  share.pin();
  share.pin();
  CHECK(!share.unheld());
  share.unpin();
  CHECK(!share.unheld());
  share.unpin();
  CHECK(share.unheld());
  share.unpin();
  CHECK(share.unheld()); }

TEST(share, holders_come_and_go_in_any_order)
{ MockSessionBackend a, b, c, stranger;
  ShareHolder first(a, 0u, 1u, 0u);
  ShareHolder second(b, 0u, 2u, 0u);
  ShareHolder third(c, 0u, 3u, 0u);
  Share share(1u, 1u, 7u, 4096u, ShareOrigin::object);
  share.let_go_by_guest();
  share.hold(first);
  share.hold(second);
  share.hold(third);
  CHECK(!share.unheld());
  CHECK(share.holder_of(a) == &first);
  CHECK(share.holder_of(b) == &second);
  CHECK(share.holder_of(c) == &third);
  CHECK(share.holder_of(stranger) == nullptr);
  share.unhold(second);
  CHECK(share.holder_of(b) == nullptr);
  CHECK(share.holder_of(a) == &first);
  CHECK(share.holder_of(c) == &third);
  CHECK(second.next() == nullptr);
  share.unhold(second);
  share.unhold(third);
  CHECK(share.holders() == &first);
  share.unhold(first);
  CHECK(share.holders() == nullptr);
  CHECK(share.unheld()); }

TEST(share, one_session_holds_it_once_for_each_thing_it_is_made_into)
{ MockSessionBackend session, other;
  ShareHolder       plain(session, 0u, 0x9001u, 0xD1u);
  ShareHolder       device(session, 0x7001u, 0x7001u, 0xD2u);
  ShareHolder       semaphore(session, 0x7002u, 0x7002u, 0xD3u);
  Share             share(1u, 1u, 7u, 4096u, ShareOrigin::object);
  share.hold(plain);
  share.hold(device);
  share.hold(semaphore);
  CHECK(share.holder_into(session, 0u) == &plain);
  CHECK(share.holder_into(session, 0x7001u) == &device);
  CHECK(share.holder_into(session, 0x7002u) == &semaphore);
  CHECK(share.holder_into(session, 0x7003u) == nullptr);
  CHECK(share.holder_into(other, 0u) == nullptr);
  CHECK(share.holder_answered(session, 0x9001u) == &plain);
  CHECK(share.holder_answered(session, 0x7002u) == &semaphore);
  CHECK(share.holder_answered(session, 0x9002u) == nullptr);
  CHECK(share.holder_answered(other, 0x9001u) == nullptr);
  share.unhold(device);
  CHECK(share.holder_into(session, 0x7001u) == nullptr);
  CHECK(share.holder_answered(session, 0x7001u) == nullptr);
  CHECK(share.holder_into(session, 0x7002u) == &semaphore);
  share.unhold(plain);
  share.unhold(semaphore);
  CHECK(share.holders() == nullptr); }

TEST(share, keeps_one_descriptor_it_owns_until_it_is_dropped)
{ Share share(1u, 1u, 7u, 4096u, ShareOrigin::object);
  const int made = ::memfd_create("share", MFD_CLOEXEC);
  CHECK(made >= 0);
  OwnedFd descriptor(made);
  share.keep_descriptor(2u, descriptor);
  CHECK(!descriptor.held());
  CHECK_EQ(share.descriptor(), made);
  CHECK_EQ(share.handle_kind(), 2u);
  CHECK(::fcntl(made, F_GETFD) != -1);
  share.drop_descriptor();
  CHECK_EQ(share.descriptor(), -1);
  CHECK_EQ(::fcntl(made, F_GETFD), -1);
  share.drop_descriptor();
  CHECK_EQ(share.descriptor(), -1); }

TEST(share, a_descriptor_kept_again_closes_the_one_before)
{ Share share(1u, 1u, 7u, 4096u, ShareOrigin::object);
  const int first = ::memfd_create("share", MFD_CLOEXEC);
  const int second = ::memfd_create("share", MFD_CLOEXEC);
  OwnedFd   one(first);
  OwnedFd   two(second);
  share.keep_descriptor(1u, one);
  share.keep_descriptor(3u, two);
  CHECK_EQ(share.descriptor(), second);
  CHECK_EQ(share.handle_kind(), 3u);
  CHECK_EQ(::fcntl(first, F_GETFD), -1); }

TEST(share, remembers_the_image_the_publisher_described)
{ Share share(1u, 1u, 7u, 4096u, ShareOrigin::object);
  CHECK(!share.shape().described());
  share.describe_image(SharedImageShape(64u, 32u, 87u, 256u));
  CHECK(share.shape().described());
  CHECK_EQ(share.shape().width(), 64u);
  CHECK_EQ(share.shape().height(), 32u);
  CHECK_EQ(share.shape().pitch(), 256u); }

TEST(share, remembers_its_publisher_without_being_held_by_it)
{ MockSessionBackend session;
  Share share(1u, 1u, 7u, 4096u, ShareOrigin::object);
  CHECK(share.publisher() == nullptr);
  share.publish_by(&session);
  CHECK(share.publisher() == &session);
  share.let_go_by_guest();
  CHECK(share.unheld());
  share.publish_by(nullptr);
  CHECK(share.publisher() == nullptr); }

TEST(share, is_indexed_until_a_reset_unindexes_it)
{ Share share(1u, 1u, 7u, 4096u, ShareOrigin::object);
  CHECK(share.indexed());
  share.unindex();
  CHECK(!share.indexed()); }
