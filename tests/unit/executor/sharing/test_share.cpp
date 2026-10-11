/* One shared object on one host GPU: what it is, what it is made of, and who
 * holds it. Freed by HostShares once no one does. */
#include "executor/sharing/share.hpp"
#include "executor/sharing/share_holder.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"

using av::CountedObject;
using av::MockSessionBackend;
using av::Share;
using av::ShareHolder;
using av::ShareOrigin;

TEST(share, names_its_object)
{ const Share share(3u, 0x2Au, 7u, 4096u, ShareOrigin::d3d12);
  CHECK_EQ(share.domain(), 3u);
  CHECK_EQ(share.id(), 0x2Au);
  CHECK_EQ(share.kind(), 7u);
  CHECK_EQ(share.size(), 4096u);
  CHECK(share.origin() == ShareOrigin::d3d12);
  CHECK_EQ(share.descriptor(), -1);
  CHECK(share.resource() == nullptr);
  CHECK(share.view() == nullptr);
  CHECK(share.shown() == nullptr);
  CHECK_EQ(share.writes(), 0u); }

/* An open that names another kind, or reaches past the object's end, would
 * have the host read what is not there. */
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

/* A caller working on a share outside the table's lock keeps it alive. */
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
  share.unpin();   /* past zero stays zero */
  CHECK(share.unheld()); }

TEST(share, holders_come_and_go_in_any_order)
{ MockSessionBackend a, b, c, stranger;
  ShareHolder first(a, 1u, 0u);
  ShareHolder second(b, 2u, 0u);
  ShareHolder third(c, 3u, 0u);
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
  share.unhold(second);                       /* the middle */
  CHECK(share.holder_of(b) == nullptr);
  CHECK(share.holder_of(a) == &first);
  CHECK(share.holder_of(c) == &third);
  CHECK(second.next() == nullptr);
  share.unhold(second);                       /* twice is nothing */
  share.unhold(third);                        /* the head */
  CHECK(share.holders() == &first);
  share.unhold(first);                        /* the last */
  CHECK(share.holders() == nullptr);
  CHECK(share.unheld()); }

TEST(share, keeps_one_descriptor_and_hands_it_over)
{ Share share(1u, 1u, 7u, 4096u, ShareOrigin::object);
  share.keep_descriptor(2u, 41);
  CHECK_EQ(share.descriptor(), 41);
  CHECK_EQ(share.handle_kind(), 2u);
  CHECK_EQ(share.take_descriptor(), 41);
  CHECK_EQ(share.descriptor(), -1);
  CHECK_EQ(share.take_descriptor(), -1); }

/* A Direct3D 11 session opens the view made over a Direct3D 12 resource, or
 * the resource itself when there is none. */
TEST(share, opens_as_its_view_or_its_resource)
{ CountedObject resource, view;
  Share share(1u, 1u, 0u, 0u, ShareOrigin::d3d12);
  share.keep_resource(&resource);
  CHECK(share.view() == &resource);
  share.keep_view(&view);
  CHECK(share.view() == &view);
  CHECK(share.resource() == &resource); }

TEST(share, gives_up_every_object_it_holds)
{ CountedObject resource, view, shown;
  Share share(1u, 1u, 0u, 0u, ShareOrigin::d3d12);
  IUnknown *objects[3] = {};
  CHECK_EQ(share.give_up_objects(objects), 0u);
  share.keep_resource(&resource);
  share.keep_shown(&shown);
  CHECK_EQ(share.give_up_objects(objects), 2u);
  CHECK(objects[0] == &resource);
  CHECK(objects[1] == &shown);
  CHECK(share.resource() == nullptr);
  CHECK(share.shown() == nullptr);
  share.keep_resource(&resource);
  share.keep_view(&view);
  share.keep_shown(&shown);
  CHECK_EQ(share.give_up_objects(objects), 3u);
  CHECK(objects[1] == &view);
  CHECK_EQ(share.give_up_objects(objects), 0u); }

TEST(share, counts_the_copies_written_into_it)
{ Share share(1u, 1u, 0u, 0u, ShareOrigin::d3d11);
  share.written();
  share.written();
  CHECK_EQ(share.writes(), 2u); }

/* Who published it is remembered to let it publish again, never as a hold. */
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

TEST(share, counts_the_direct3d_opens_of_it)
{ Share share(1u, 1u, 0u, 0u, ShareOrigin::d3d11);
  CHECK_EQ(share.opens(), 0u);
  share.opened();
  share.opened();
  CHECK_EQ(share.opens(), 2u); }
