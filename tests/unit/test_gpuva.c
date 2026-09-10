/* Every branch of src/core/gpuva.c, the module's invariants after every
 * mutation, and hostile input. Constitution Principle III. */
#include "../harness/av_test.h"
#include "../harness/av_hostile.h"
#include "../harness/platform_mock.h"
#include "../../src/core/gpuva.h"

#define VA_BASE 0x1000000000ull
#define VA_SIZE 0x0000100000ull          /* 1 MiB */
#define PAGE    AV_GPUVA_PAGE_SIZE

static void mk(av_gpuva_space_t *s)
{
    CHECK_STATUS(av_gpuva_init(s, VA_BASE, VA_SIZE), AV_OK);
    CHECK(av_gpuva_invariants(s));
}

/* Every mutation is followed by an invariant check, per the constitution. */
#define MUTATE(s, expr, want) do { CHECK_STATUS(expr, want);                   \
                                   CHECK(av_gpuva_invariants(s)); } while (0)

/* ---------------------------------------------------------------- init ---- */

TEST(gpuva_init, rejects_null_space)
{ CHECK_STATUS(av_gpuva_init(NULL, VA_BASE, VA_SIZE), AV_E_INVALID_PARAM); }

TEST(gpuva_init, rejects_zero_size)
{ av_gpuva_space_t s; CHECK_STATUS(av_gpuva_init(&s, VA_BASE, 0), AV_E_INVALID_PARAM); }

TEST(gpuva_init, rejects_unaligned_base)
{ av_gpuva_space_t s; CHECK_STATUS(av_gpuva_init(&s, VA_BASE + 1, VA_SIZE), AV_E_INVALID_PARAM); }

TEST(gpuva_init, rejects_unaligned_size)
{ av_gpuva_space_t s; CHECK_STATUS(av_gpuva_init(&s, VA_BASE, VA_SIZE + 1), AV_E_INVALID_PARAM); }

TEST(gpuva_init, rejects_base_plus_size_overflow)
{ av_gpuva_space_t s;
  CHECK_STATUS(av_gpuva_init(&s, 0xFFFFFFFFFFFFF000ull, 0x2000), AV_E_INVALID_PARAM); }

TEST(gpuva_init, accepts_aligned_range_and_starts_empty)
{ av_gpuva_space_t s; mk(&s);
  CHECK_EQ(s.count, 0); CHECK_EQ(s.capacity, 0);
  CHECK_EQ(s.max_ranges, AV_GPUVA_MAX_RANGES);
  av_gpuva_destroy(&s); }

/* ------------------------------------------------------------- destroy ---- */

TEST(gpuva_destroy, tolerates_null)      { av_gpuva_destroy(NULL); }
TEST(gpuva_destroy, tolerates_no_ranges) { av_gpuva_space_t s; mk(&s); av_gpuva_destroy(&s); }

TEST(gpuva_destroy, releases_the_table)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  av_gpuva_destroy(&s);
  CHECK_EQ(s.count, 0); CHECK_EQ(s.capacity, 0); CHECK(s.ranges == NULL); }

/* ------------------------------------------------------------- reserve ---- */

TEST(gpuva_reserve, rejects_null_space)
{ uint64_t b; CHECK_STATUS(av_gpuva_reserve(NULL, PAGE, PAGE, &b), AV_E_INVALID_PARAM); }

TEST(gpuva_reserve, rejects_null_out)
{ av_gpuva_space_t s; mk(&s);
  CHECK_STATUS(av_gpuva_reserve(&s, PAGE, PAGE, NULL), AV_E_INVALID_PARAM);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, rejects_zero_size)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  CHECK_STATUS(av_gpuva_reserve(&s, 0, PAGE, &b), AV_E_INVALID_PARAM);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, rejects_unaligned_size)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  CHECK_STATUS(av_gpuva_reserve(&s, PAGE + 1, PAGE, &b), AV_E_INVALID_PARAM);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, rejects_non_power_of_two_alignment)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  CHECK_STATUS(av_gpuva_reserve(&s, PAGE, PAGE * 3, &b), AV_E_INVALID_PARAM);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, rejects_zero_alignment)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  CHECK_STATUS(av_gpuva_reserve(&s, PAGE, 0, &b), AV_E_INVALID_PARAM);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, rejects_alignment_below_page_size)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  CHECK_STATUS(av_gpuva_reserve(&s, PAGE, 512, &b), AV_E_INVALID_PARAM);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, rejects_size_larger_than_space)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  CHECK_STATUS(av_gpuva_reserve(&s, VA_SIZE + PAGE, PAGE, &b), AV_E_TOO_LARGE);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, first_reservation_lands_at_base)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  CHECK_EQ(b, VA_BASE); CHECK_EQ(s.count, 1);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, second_reservation_follows_the_first)
{ av_gpuva_space_t s; uint64_t a, b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  CHECK_EQ(b, a + PAGE);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, reuses_a_hole_left_by_free)
{ av_gpuva_space_t s; uint64_t a, b, c, d; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &c), AV_OK);
  MUTATE(&s, av_gpuva_free(&s, b), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &d), AV_OK);
  CHECK_EQ(d, b);                       /* the hole, not the tail */
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, honours_alignment_larger_than_a_page)
{ av_gpuva_space_t s; uint64_t a, b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE * 16, &b), AV_OK);
  CHECK_EQ(b % (PAGE * 16), 0);
  CHECK(b > a);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, fills_the_space_exactly_then_refuses)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, VA_SIZE, PAGE, &b), AV_OK);
  CHECK_EQ(b, VA_BASE);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_E_NO_SPACE);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, refuses_when_alignment_pushes_past_the_end)
{ av_gpuva_space_t s; uint64_t a, b; mk(&s);
  /* occupy everything but the last page, then demand a huge alignment */
  MUTATE(&s, av_gpuva_reserve(&s, VA_SIZE - PAGE, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, VA_SIZE * 4, &b), AV_E_NO_SPACE);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, propagates_allocation_failure)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  av_mock_fail_alloc_at(1);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_E_NO_MEMORY);
  CHECK_EQ(s.count, 0);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, grows_the_table_past_the_initial_capacity)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  for (int i = 0; i < 20; i++) MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  CHECK_EQ(s.count, 20); CHECK(s.capacity >= 20);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, refuses_when_the_table_ceiling_is_reached)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  s.max_ranges = 16;                     /* lowered so the branch is reachable */
  for (int i = 0; i < 16; i++) MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_E_NO_SPACE);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, clamps_growth_to_the_ceiling)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  s.max_ranges = 20;                     /* 16 -> clamp to 20, not 32 */
  for (int i = 0; i < 17; i++) MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  CHECK_EQ(s.capacity, 20);
  av_gpuva_destroy(&s); }

/* ----------------------------------------------------------------- map ---- */

TEST(gpuva_map, rejects_null_space)
{ CHECK_STATUS(av_gpuva_map(NULL, VA_BASE, PAGE, 1, 0), AV_E_INVALID_PARAM); }

TEST(gpuva_map, rejects_object_zero_because_zero_means_unmapped)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, b, PAGE, 0, 0), AV_E_INVALID_PARAM);
  av_gpuva_destroy(&s); }

TEST(gpuva_map, rejects_offset_plus_size_overflow)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, b, PAGE, 7, UINT64_MAX), AV_E_INVALID_PARAM);
  av_gpuva_destroy(&s); }

TEST(gpuva_map, rejects_unreserved_base)
{ av_gpuva_space_t s; mk(&s);
  MUTATE(&s, av_gpuva_map(&s, VA_BASE, PAGE, 7, 0), AV_E_NOT_FOUND);
  av_gpuva_destroy(&s); }

TEST(gpuva_map, rejects_size_that_does_not_match_the_reservation)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE * 2, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, b, PAGE, 7, 0), AV_E_INVALID_PARAM);
  av_gpuva_destroy(&s); }

TEST(gpuva_map, rejects_double_map)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, b, PAGE, 7, 0), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, b, PAGE, 8, 0), AV_E_CONFLICT);
  av_gpuva_destroy(&s); }

TEST(gpuva_map, binds_object_and_offset)
{ av_gpuva_space_t s; uint64_t b; uint32_t obj; uint64_t off; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, b, PAGE, 7, 0x2000), AV_OK);
  CHECK_STATUS(av_gpuva_translate(&s, b, &obj, &off), AV_OK);
  CHECK_EQ(obj, 7); CHECK_EQ(off, 0x2000);
  av_gpuva_destroy(&s); }

/* --------------------------------------------------------------- unmap ---- */

TEST(gpuva_unmap, rejects_null_space)
{ CHECK_STATUS(av_gpuva_unmap(NULL, VA_BASE), AV_E_INVALID_PARAM); }

TEST(gpuva_unmap, rejects_unknown_base)
{ av_gpuva_space_t s; mk(&s);
  MUTATE(&s, av_gpuva_unmap(&s, VA_BASE), AV_E_NOT_FOUND);
  av_gpuva_destroy(&s); }

TEST(gpuva_unmap, rejects_range_that_was_never_mapped)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_unmap(&s, b), AV_E_INVALID_STATE);
  av_gpuva_destroy(&s); }

TEST(gpuva_unmap, clears_the_binding_and_allows_remap)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, b, PAGE, 7, 0), AV_OK);
  MUTATE(&s, av_gpuva_unmap(&s, b), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, b, PAGE, 9, 0), AV_OK);
  av_gpuva_destroy(&s); }

/* ---------------------------------------------------------------- free ---- */

TEST(gpuva_free, rejects_null_space)
{ CHECK_STATUS(av_gpuva_free(NULL, VA_BASE), AV_E_INVALID_PARAM); }

TEST(gpuva_free, rejects_unknown_base)
{ av_gpuva_space_t s; mk(&s);
  MUTATE(&s, av_gpuva_free(&s, VA_BASE), AV_E_NOT_FOUND);
  av_gpuva_destroy(&s); }

TEST(gpuva_free, rejects_a_base_past_every_range)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_free(&s, b + PAGE * 4), AV_E_NOT_FOUND);
  av_gpuva_destroy(&s); }

TEST(gpuva_free, refuses_while_still_mapped)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, b, PAGE, 7, 0), AV_OK);
  MUTATE(&s, av_gpuva_free(&s, b), AV_E_INVALID_STATE);
  av_gpuva_destroy(&s); }

TEST(gpuva_free, removes_the_last_range)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_free(&s, b), AV_OK);
  CHECK_EQ(s.count, 0);
  av_gpuva_destroy(&s); }

TEST(gpuva_free, removes_a_middle_range_and_keeps_order)
{ av_gpuva_space_t s; uint64_t a, b, c; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &c), AV_OK);
  MUTATE(&s, av_gpuva_free(&s, b), AV_OK);
  CHECK_EQ(s.count, 2);
  CHECK_EQ(s.ranges[0].base, a); CHECK_EQ(s.ranges[1].base, c);
  av_gpuva_destroy(&s); }

/* ----------------------------------------------------------- translate ---- */

TEST(gpuva_translate, rejects_null_arguments)
{ av_gpuva_space_t s; uint32_t o; uint64_t f; mk(&s);
  CHECK_STATUS(av_gpuva_translate(NULL, VA_BASE, &o, &f), AV_E_INVALID_PARAM);
  CHECK_STATUS(av_gpuva_translate(&s, VA_BASE, NULL, &f), AV_E_INVALID_PARAM);
  CHECK_STATUS(av_gpuva_translate(&s, VA_BASE, &o, NULL), AV_E_INVALID_PARAM);
  av_gpuva_destroy(&s); }

TEST(gpuva_translate, rejects_an_empty_space)
{ av_gpuva_space_t s; uint32_t o; uint64_t f; mk(&s);
  CHECK_STATUS(av_gpuva_translate(&s, VA_BASE, &o, &f), AV_E_NOT_FOUND);
  av_gpuva_destroy(&s); }

TEST(gpuva_translate, rejects_an_address_below_every_range)
{ av_gpuva_space_t s; uint64_t a, b; uint32_t o; uint64_t f; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_free(&s, a), AV_OK);
  CHECK_STATUS(av_gpuva_translate(&s, VA_BASE, &o, &f), AV_E_NOT_FOUND);
  av_gpuva_destroy(&s); }

TEST(gpuva_translate, rejects_an_address_in_a_hole_between_ranges)
{ av_gpuva_space_t s; uint64_t a, b, c; uint32_t o; uint64_t f; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &c), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, a, PAGE, 1, 0), AV_OK);
  MUTATE(&s, av_gpuva_free(&s, b), AV_OK);
  CHECK_STATUS(av_gpuva_translate(&s, b, &o, &f), AV_E_NOT_FOUND);
  av_gpuva_destroy(&s); }

TEST(gpuva_translate, rejects_an_address_past_the_last_range)
{ av_gpuva_space_t s; uint64_t a; uint32_t o; uint64_t f; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, a, PAGE, 1, 0), AV_OK);
  CHECK_STATUS(av_gpuva_translate(&s, a + PAGE, &o, &f), AV_E_NOT_FOUND);
  av_gpuva_destroy(&s); }

TEST(gpuva_translate, rejects_a_reserved_but_unmapped_range)
{ av_gpuva_space_t s; uint64_t a; uint32_t o; uint64_t f; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &a), AV_OK);
  CHECK_STATUS(av_gpuva_translate(&s, a, &o, &f), AV_E_INVALID_STATE);
  av_gpuva_destroy(&s); }

TEST(gpuva_translate, adds_the_intra_range_offset)
{ av_gpuva_space_t s; uint64_t a; uint32_t o; uint64_t f; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE * 2, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, a, PAGE * 2, 7, 0x1000), AV_OK);
  CHECK_STATUS(av_gpuva_translate(&s, a + 0x800, &o, &f), AV_OK);
  CHECK_EQ(o, 7); CHECK_EQ(f, 0x1800);
  CHECK_STATUS(av_gpuva_translate(&s, a + PAGE * 2 - 1, &o, &f), AV_OK);
  CHECK_EQ(f, 0x1000 + PAGE * 2 - 1);
  av_gpuva_destroy(&s); }

TEST(gpuva_translate, is_callable_at_dispatch_level)
{ av_gpuva_space_t s; uint64_t a; uint32_t o; uint64_t f; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_map(&s, a, PAGE, 7, 0), AV_OK);
  av_mock_set_irql(AV_IRQL_DISPATCH);
  CHECK_STATUS(av_gpuva_translate(&s, a, &o, &f), AV_OK);
  CHECK(!av_mock_irql_violated());
  av_mock_set_irql(AV_IRQL_PASSIVE);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, is_rejected_at_dispatch_level)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  av_mock_set_irql(AV_IRQL_DISPATCH);
  (void)av_gpuva_reserve(&s, PAGE, PAGE, &b);
  CHECK(av_mock_irql_violated());        /* the annotation is enforced */
  av_mock_set_irql(AV_IRQL_PASSIVE);
  av_gpuva_destroy(&s); }

/* ---------------------------------------------------------- invariants ---- */

TEST(gpuva_invariants, reject_a_null_space)      { CHECK(!av_gpuva_invariants(NULL)); }

TEST(gpuva_invariants, reject_a_corrupted_table)
{ av_gpuva_space_t s; uint64_t a, b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);

  s.ranges[1].base = s.ranges[0].base;            CHECK(!av_gpuva_invariants(&s));
  s.ranges[1].base = b;                           CHECK(av_gpuva_invariants(&s));
  s.ranges[0].size = 0;                           CHECK(!av_gpuva_invariants(&s));
  s.ranges[0].size = PAGE;                        CHECK(av_gpuva_invariants(&s));
  s.ranges[0].base += 1;                          CHECK(!av_gpuva_invariants(&s));
  s.ranges[0].base -= 1;                          CHECK(av_gpuva_invariants(&s));
  s.ranges[0].size += 1;                          CHECK(!av_gpuva_invariants(&s));
  s.ranges[0].size -= 1;                          CHECK(av_gpuva_invariants(&s));
  s.ranges[0].object = 5;                         CHECK(!av_gpuva_invariants(&s));
  s.ranges[0].object = 0;                         CHECK(av_gpuva_invariants(&s));
  s.ranges[0].object_offset = 8;                  CHECK(!av_gpuva_invariants(&s));
  s.ranges[0].object_offset = 0;                  CHECK(av_gpuva_invariants(&s));
  s.ranges[1].size = VA_SIZE;                     CHECK(!av_gpuva_invariants(&s));
  s.ranges[1].size = PAGE;                        CHECK(av_gpuva_invariants(&s));
  s.count = s.capacity + 1;                       CHECK(!av_gpuva_invariants(&s));
  s.count = 2;                                    CHECK(av_gpuva_invariants(&s));
  s.max_ranges = 0;                               CHECK(!av_gpuva_invariants(&s));
  s.max_ranges = AV_GPUVA_MAX_RANGES + 1;         CHECK(!av_gpuva_invariants(&s));
  s.max_ranges = 1;                               CHECK(!av_gpuva_invariants(&s));
  s.max_ranges = AV_GPUVA_MAX_RANGES;             CHECK(av_gpuva_invariants(&s));
  s.va_size = UINT64_MAX;                         CHECK(!av_gpuva_invariants(&s));
  s.va_size = VA_SIZE;                            CHECK(av_gpuva_invariants(&s));
  av_gpuva_destroy(&s);

  av_gpuva_space_t t; mk(&t);
  t.capacity = 4; t.ranges = NULL;                CHECK(!av_gpuva_invariants(&t));
  t.capacity = 0;
  av_gpuva_destroy(&t); }

TEST(gpuva_invariants, hold_across_a_pseudorandom_operation_sequence)
{ av_gpuva_space_t s; mk(&s);
  uint64_t held[64]; uint32_t n = 0; uint32_t rng = 0xC0FFEEu;
  for (int i = 0; i < 4000; i++) {
      rng = rng * 1664525u + 1013904223u;
      uint32_t op = (rng >> 16) & 3u;
      if (op == 0u && n < 64u) {
          uint64_t b;
          uint64_t sz = (uint64_t)PAGE * (1u + ((rng >> 4) & 3u));
          if (av_gpuva_reserve(&s, sz, PAGE, &b) == AV_OK) held[n++] = b;
      } else if (op == 1u && n > 0u) {
          uint32_t k = (rng >> 8) % n;
          (void)av_gpuva_map(&s, held[k], s.ranges[0].size, 1u + k, 0u);
      } else if (op == 2u && n > 0u) {
          uint32_t k = (rng >> 8) % n;
          (void)av_gpuva_unmap(&s, held[k]);
      } else if (n > 0u) {
          uint32_t k = (rng >> 8) % n;
          if (av_gpuva_free(&s, held[k]) == AV_OK) held[k] = held[--n];
      }
      CHECK(av_gpuva_invariants(&s));    /* after EVERY mutation */
  }
  av_gpuva_destroy(&s); }

/* -------------------------------------------------- allocation failure ---- */

TEST(gpuva, survives_failure_at_every_allocation_site)
{ /* Sweep: fail the Nth allocation, assert no crash and no leak. */
  for (uint32_t nth = 1; nth <= 8; nth++) {
      av_gpuva_space_t s; uint64_t b;
      CHECK_STATUS(av_gpuva_init(&s, VA_BASE, VA_SIZE), AV_OK);
      av_mock_fail_alloc_at(nth);
      for (int i = 0; i < 40; i++) {
          if (av_gpuva_reserve(&s, PAGE, PAGE, &b) != AV_OK) break;
      }
      CHECK(av_gpuva_invariants(&s));
      av_gpuva_destroy(&s);
      av_mock_fail_alloc_at(0);
  } }

/* ------------------------------------------------------- hostile input ---- */

/* The submission path receives a GPU VA that came from a user-mode process.
 * Translation must reject anything it does not own, for every possible value. */
static av_gpuva_space_t g_hostile;

static int parse_va(const uint8_t *p, size_t len)
{
    uint64_t va = 0;
    for (size_t i = 0; i < len && i < 8; i++) va |= (uint64_t)p[i] << (8 * i);
    uint32_t obj; uint64_t off;
    av_status_t st = av_gpuva_translate(&g_hostile, va, &obj, &off);
    if (st == AV_OK) {
        /* A success must be inside a mapped range, always. */
        CHECK(obj != 0u);
    }
    return st != AV_OK;
}

TEST(gpuva_translate, hostile_addresses_never_translate_out_of_bounds)
{ uint64_t a;
  CHECK_STATUS(av_gpuva_init(&g_hostile, VA_BASE, VA_SIZE), AV_OK);
  CHECK_STATUS(av_gpuva_reserve(&g_hostile, PAGE * 4, PAGE, &a), AV_OK);
  CHECK_STATUS(av_gpuva_map(&g_hostile, a, PAGE * 4, 42, 0x10000), AV_OK);

  uint8_t valid[8];
  for (int i = 0; i < 8; i++) valid[i] = (uint8_t)(a >> (8 * i));
  av_hostile_sweep(parse_va, valid, sizeof valid);

  /* the specific adversarial values, named */
  uint32_t o; uint64_t f;
  CHECK_STATUS(av_gpuva_translate(&g_hostile, 0, &o, &f), AV_E_NOT_FOUND);
  CHECK_STATUS(av_gpuva_translate(&g_hostile, UINT64_MAX, &o, &f), AV_E_NOT_FOUND);
  CHECK_STATUS(av_gpuva_translate(&g_hostile, a - 1, &o, &f), AV_E_NOT_FOUND);
  CHECK_STATUS(av_gpuva_translate(&g_hostile, a + PAGE * 4, &o, &f), AV_E_NOT_FOUND);
  CHECK_STATUS(av_gpuva_translate(&g_hostile, a + PAGE * 4 - 1, &o, &f), AV_OK);
  CHECK_EQ(f, 0x10000 + PAGE * 4 - 1);
  av_gpuva_destroy(&g_hostile); }

/* ------------------------------------------------ closing the last gaps ---- */

TEST(gpuva_align_up, rejects_null_out)
{ CHECK(!av_gpuva_align_up(0, PAGE, NULL)); }

TEST(gpuva_align_up, rejects_non_power_of_two)
{ uint64_t o; CHECK(!av_gpuva_align_up(0, 3, &o)); CHECK(!av_gpuva_align_up(0, 0, &o)); }

TEST(gpuva_align_up, reports_overflow_instead_of_wrapping)
{ uint64_t o;
  CHECK(!av_gpuva_align_up(UINT64_MAX, PAGE, &o));
  CHECK(!av_gpuva_align_up(UINT64_MAX - PAGE + 2, PAGE, &o));
  CHECK(av_gpuva_align_up(UINT64_MAX - PAGE + 1, PAGE, &o));
  CHECK_EQ(o, UINT64_MAX - PAGE + 1); }

TEST(gpuva_align_up, rounds_up_and_leaves_aligned_values_alone)
{ uint64_t o;
  CHECK(av_gpuva_align_up(1, PAGE, &o));    CHECK_EQ(o, PAGE);
  CHECK(av_gpuva_align_up(PAGE, PAGE, &o)); CHECK_EQ(o, PAGE); }

TEST(gpuva_map, rejects_a_base_that_falls_before_an_existing_range)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  /* lower_bound lands on range 0, whose base differs: the second half of the
   * find_exact conjunction. */
  MUTATE(&s, av_gpuva_map(&s, b - PAGE, PAGE, 7, 0), AV_E_NOT_FOUND);
  av_gpuva_destroy(&s); }

TEST(gpuva_free, rejects_a_base_that_falls_before_an_existing_range)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_free(&s, b - PAGE), AV_E_NOT_FOUND);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, refuses_when_only_the_tail_hole_is_left_and_it_is_too_small)
{ av_gpuva_space_t s; uint64_t a, b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, VA_SIZE - PAGE, PAGE, &a), AV_OK);
  /* one page of tail remains; ask for two, so the search runs off the end */
  MUTATE(&s, av_gpuva_reserve(&s, PAGE * 2, PAGE, &b), AV_E_NO_SPACE);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, refuses_when_the_initial_cursor_alignment_overflows)
{ av_gpuva_space_t s; uint64_t b;
  CHECK_STATUS(av_gpuva_init(&s, 0xFFFFFFFFFFF00000ull, 0xF0000ull), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, 0x8000000000000000ull, &b), AV_E_NO_SPACE);
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, refuses_when_aligning_past_a_range_overflows)
{ av_gpuva_space_t s; uint64_t a, b;
  CHECK_STATUS(av_gpuva_init(&s, 0xFFFFFFFFFFF00000ull, 0xF0000ull), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, 0x100000ull, &a), AV_OK);
  /* aligning past the end of that range lands beyond UINT64_MAX */
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, 0x100000ull, &b), AV_E_NO_SPACE);
  av_gpuva_destroy(&s); }

TEST(gpuva_invariants, reject_a_zero_ceiling_on_an_empty_table)
{ av_gpuva_space_t s; mk(&s);
  s.max_ranges = 0;                    /* capacity is 0, so this is the first
                                        * check that can see it */
  CHECK(!av_gpuva_invariants(&s));
  av_gpuva_destroy(&s); }

TEST(gpuva_invariants, reject_ranges_whose_arithmetic_overflows)
{ av_gpuva_space_t s; uint64_t b; mk(&s);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);

  s.ranges[0].size = 0xFFFFFFFFFFFFF000ull;   /* base + size overflows */
  CHECK(!av_gpuva_invariants(&s));
  s.ranges[0].size = PAGE;
  CHECK(av_gpuva_invariants(&s));

  s.ranges[0].object = 1;                     /* mapped, with a hostile offset */
  s.ranges[0].mapped = true;
  s.ranges[0].object_offset = 0xFFFFFFFFFFFFF000ull;
  CHECK(!av_gpuva_invariants(&s));
  s.ranges[0].object_offset = 0;
  CHECK(av_gpuva_invariants(&s));
  s.ranges[0].object = 0; s.ranges[0].mapped = false;
  av_gpuva_destroy(&s); }

TEST(gpuva_reserve, does_not_rewind_the_cursor_when_alignment_skips_a_range)
{ av_gpuva_space_t s; uint64_t a, b, c, d; mk(&s);
  /* three adjacent single pages, then a request aligned far past all of them:
   * the cursor is already beyond range 1's aligned end, so it must not move
   * backwards. */
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &a), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &b), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE, &c), AV_OK);
  MUTATE(&s, av_gpuva_reserve(&s, PAGE, PAGE * 16, &d), AV_OK);
  CHECK_EQ(d, VA_BASE + PAGE * 16);
  av_gpuva_destroy(&s); }
