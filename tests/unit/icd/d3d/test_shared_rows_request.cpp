/* Which threads asked that the shared textures they make be laid out as rows:
 * a slot per asking thread, an ask that finds none answers so, and thread id
 * 0 is never one. */
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/shared_rows_request.hpp"

#include <cstdint>

using av::d3d::SharedRowsRequest;

TEST(shared_rows_request, a_thread_asks_for_rows_until_it_withdraws)
{ SharedRowsRequest request;
  CHECK(!request.asked_by(41u));
  CHECK(request.ask(41u));
  CHECK(request.asked_by(41u));
  /* Another thread's textures are tiled as ever. */
  CHECK(!request.asked_by(42u));
  request.withdraw(41u);
  CHECK(!request.asked_by(41u));
  /* Withdrawing what was never asked changes nothing. */
  request.withdraw(77u);
  CHECK(!request.asked_by(77u)); }

TEST(shared_rows_request, asking_twice_takes_one_slot)
{ SharedRowsRequest request;
  CHECK(request.ask(5u));
  CHECK(request.ask(5u));
  for (std::uint32_t thread = 100u; thread < 100u + SharedRowsRequest::most_threads - 1u; thread++)
      CHECK(request.ask(thread));
  request.withdraw(5u);
  CHECK(!request.asked_by(5u)); }

TEST(shared_rows_request, a_thread_past_every_slot_is_refused_and_tiled)
{ SharedRowsRequest request;
  for (std::uint32_t thread = 1u; thread <= SharedRowsRequest::most_threads; thread++) CHECK(request.ask(thread));
  CHECK(!request.ask(1000u));
  CHECK(!request.asked_by(1000u));
  /* A slot let go is taken by the next. */
  request.withdraw(3u);
  CHECK(request.ask(1000u));
  CHECK(request.asked_by(1000u));
  CHECK(request.asked_by(SharedRowsRequest::most_threads)); }

TEST(shared_rows_request, thread_zero_never_asks)
{ SharedRowsRequest request;
  CHECK(!request.ask(0u));
  CHECK(!request.asked_by(0u));
  /* Empty slots hold zero, and still do not read as asked. */
  CHECK(request.ask(9u));
  CHECK(!request.asked_by(0u));
  request.withdraw(0u);
  CHECK(request.asked_by(9u)); }
