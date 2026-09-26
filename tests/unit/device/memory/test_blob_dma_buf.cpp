/* One dma-buf over a guest blob's scattered pages, or over a host blob's
 * memory (docs/host-window.md).
 *
 * The alternative to a host pointer, and the one that works where the GPU
 * driver refuses to pin a userptr whose pages are file-backed - which guest
 * RAM always is. The blob's extents become the dma-buf's items, in order, so
 * a host GPU sees one contiguous range and no byte is copied.
 *
 * Every test runs over MockHost's file-backed guest RAM, through the kernel's
 * own udmabuf, so a dma-buf here is a real one. A kernel without udmabuf
 * refuses, and the tests say so rather than pretending: that refusal is the
 * caller's signal to import a pointer instead, and it has to be survivable. */
#include "core/support/status.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/blob_dma_buf.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

using av::BlobWindow;
using av::BlobDmaBuf;
using av::MockHost;
using av::Status;

namespace av {

/* States a dma-buf never builds for itself: a descriptor with no length, a
 * length with no descriptor, more items than one may carry. */
struct BlobDmaBufProbe {
    static void set(BlobDmaBuf &target, int handle, std::uint64_t length, std::uint32_t items)
    { target.handle_ = handle; target.length_ = length; target.items_ = items; }
};

}  // namespace av

namespace {

constexpr std::uint64_t page = BlobDmaBuf::page_size;
constexpr std::uint64_t ram = 0x40000000u;
constexpr std::uint32_t ram_pages = 64u;

class Extents {
public:
    explicit Extents(std::uint32_t capacity)
        : pointers_(static_cast<void **>(std::calloc(capacity, sizeof(void *)))),
          lengths_(static_cast<std::uint64_t *>(std::calloc(capacity, sizeof(std::uint64_t)))),
          count_(0u), length_(0u) {}
    ~Extents() { std::free(pointers_); std::free(lengths_); }
    Extents(const Extents &) = delete;
    Extents &operator=(const Extents &) = delete;

    Extents &add(void *pointer, std::uint64_t bytes)
    {
        pointers_[count_] = pointer;
        lengths_[count_] = bytes;
        count_++;
        length_ += bytes;
        return *this;
    }
    BlobWindow window() const { return BlobWindow(pointers_, lengths_, count_, length_); }

private:
    void         **pointers_;
    std::uint64_t *lengths_;
    std::uint32_t  count_;
    std::uint64_t  length_;
};

class Fixture {
public:
    Fixture() { host_.add_guest_ram(ram, ram_pages * page); }
    MockHost &host() { return host_; }
    void     *guest_page(std::uint32_t index) { return host_.guest_ram(ram + index * page); }

private:
    MockHost host_;
};

/* Whether this kernel makes dma-bufs at all. A test that needs one says so
 * rather than failing on a machine that cannot. */
bool udmabuf_here(Fixture &rig)
{
    Extents     one(1u);
    one.add(rig.guest_page(0u), page);
    BlobDmaBuf probe(rig.host());
    const bool  made = probe.make(one.window(), 0u, page) == Status::ok;
    probe.release();
    return made;
}

}  // namespace

TEST(blob_dma_buf, a_new_one_holds_nothing)
{ Fixture     rig;
  BlobDmaBuf buffer(rig.host());
  CHECK(!buffer.made());
  CHECK_EQ(buffer.handle(), -1);
  CHECK_EQ(buffer.length(), 0u);
  CHECK_EQ(buffer.items(), 0u);
  CHECK(buffer.invariants_hold());
  /* Releasing one that holds nothing is not a close of anything. */
  buffer.release();
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(blob_dma_buf, scattered_pages_become_one_dma_buf_in_the_order_given)
{ Fixture rig;
  if (!udmabuf_here(rig)) return;
  /* Three runs, out of order in guest RAM and of different lengths: what a
   * guest allocator actually hands back. */
  Extents pages(3u);
  pages.add(rig.guest_page(9u), 2u * page)
       .add(rig.guest_page(1u), page)
       .add(rig.guest_page(4u), 3u * page);
  BlobDmaBuf buffer(rig.host());
  CHECK_STATUS(buffer.make(pages.window(), 0u, 6u * page), Status::ok);
  CHECK(buffer.made());
  CHECK(buffer.handle() >= 0);
  CHECK_EQ(buffer.length(), 6u * page);
  CHECK_EQ(buffer.items(), 3u);
  CHECK_EQ(rig.host().live_dmabufs(), 1u);
  CHECK(buffer.invariants_hold());

  /* A second make is refused and changes nothing: the first is still the
   * one this holds. */
  const int held = buffer.handle();
  CHECK_STATUS(buffer.make(pages.window(), 0u, 6u * page), Status::invalid_state);
  CHECK_EQ(buffer.handle(), held);
  CHECK_EQ(rig.host().live_dmabufs(), 1u);

  buffer.release();
  CHECK(!buffer.made());
  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  CHECK(buffer.invariants_hold()); }

TEST(blob_dma_buf, the_destructor_closes_what_the_caller_did_not)
{ Fixture rig;
  if (!udmabuf_here(rig)) return;
  { Extents pages(1u);
    pages.add(rig.guest_page(0u), 6u * page);
    BlobDmaBuf buffer(rig.host());
    CHECK_STATUS(buffer.make(pages.window(), 0u, 6u * page), Status::ok);
    CHECK_EQ(rig.host().live_dmabufs(), 1u); }
  /* A descriptor left open is one the process runs out of. */
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(blob_dma_buf, a_window_that_is_not_whole_pages_is_refused_before_the_host)
{ Fixture rig;
  Extents  none(1u);
  BlobDmaBuf empty(rig.host());
  /* No extent at all. */
  CHECK_STATUS(empty.make(none.window(), 0u, page), Status::too_large);

  Extents partial(1u);
  partial.add(rig.guest_page(0u), page + 1u);
  BlobDmaBuf ragged(rig.host());
  CHECK_STATUS(ragged.make(partial.window(), 0u, page + 1u), Status::malformed);

  Extents nothing(1u);
  nothing.add(rig.guest_page(0u), 0u);
  BlobDmaBuf sizeless(rig.host());
  CHECK_STATUS(sizeless.make(nothing.window(), 0u, 0u), Status::malformed);

  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  CHECK(empty.invariants_hold());
  CHECK(ragged.invariants_hold());
  CHECK(sizeless.invariants_hold()); }

/* An extent the blob table would never hand out: the walk judges each one it
 * reaches rather than trusting the window, because what it builds is an ioctl
 * the kernel reads. Each is the first extent of a range whose total is whole
 * pages, so the walk reaches it. */
TEST(blob_dma_buf, an_extent_that_is_no_run_of_pages_is_refused_on_the_walk)
{ Fixture rig;
  struct Case { void *base; std::uint64_t bytes; };
  const Case bad[] = {{nullptr, page}, {nullptr, 0u}, {nullptr, page + 8u}};
  for (std::uint32_t index = 0u; index < 3u; index++) {
      Extents pages(2u);
      pages.add(index == 0u ? nullptr : rig.guest_page(0u), bad[index].bytes)
           .add(rig.guest_page(2u), 2u * page - (index == 2u ? 8u : 0u));
      BlobDmaBuf buffer(rig.host());
      CHECK_STATUS(buffer.make(pages.window(), 0u, 2u * page), Status::malformed);
      CHECK(!buffer.made());
      CHECK_EQ(rig.host().live_dmabufs(), 0u);
  } }

/* More runs than one dma-buf may carry: a blob scattered further than the
 * kernel's list can hold is answered, not truncated. */
TEST(blob_dma_buf, more_extents_than_items_is_no_memory)
{ Fixture rig;
  Extents pages(BlobDmaBuf::max_items + 1u);
  for (std::uint32_t index = 0u; index <= BlobDmaBuf::max_items; index++)
      pages.add(rig.guest_page(index % ram_pages), page);
  BlobDmaBuf buffer(rig.host());
  CHECK_STATUS(buffer.make(pages.window(), 0u, (BlobDmaBuf::max_items + 1u) * page), Status::no_memory);
  CHECK(!buffer.made());
  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  CHECK(buffer.invariants_hold()); }

TEST(blob_dma_buf, a_host_that_cannot_make_one_is_answered_rather_than_worked_around)
{ Fixture rig;
  Extents pages(1u);
  pages.add(rig.guest_page(0u), page);
  BlobDmaBuf buffer(rig.host());
  /* A kernel with no udmabuf looks exactly like this, and the caller reads
   * it as "import a pointer instead" rather than as a failure of the blob. */
  rig.host().fail_dmabuf_at(1u);
  CHECK_STATUS(buffer.make(pages.window(), 0u, page), Status::not_supported);
  CHECK(!buffer.made());
  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  CHECK(buffer.invariants_hold()); }

TEST(blob_dma_buf, memory_the_host_has_no_file_for_makes_no_dma_buf)
{ Fixture rig;
  /* The harness's own memory, not guest RAM: no fd behind it, which is what
   * QEMU says of RAM it cannot share. */
  auto *const mine = static_cast<std::uint8_t *>(std::aligned_alloc(page, page));
  CHECK(mine != nullptr);
  if (mine == nullptr) return;
  Extents pages(1u);
  pages.add(mine, page);
  BlobDmaBuf buffer(rig.host());
  CHECK_STATUS(buffer.make(pages.window(), 0u, page), Status::not_supported);
  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  CHECK(buffer.invariants_hold());
  std::free(mine); }

/* A range inside the blob, which is what an import asks for: the dma-buf
 * carries those bytes and no others, because a Vulkan fd import names no
 * offset and whoever imports it gets the whole descriptor. */
TEST(blob_dma_buf, a_range_inside_the_blob_carries_that_range_alone)
{ Fixture rig;
  if (!udmabuf_here(rig)) return;
  Extents pages(3u);
  pages.add(rig.guest_page(9u), 2u * page)
       .add(rig.guest_page(1u), page)
       .add(rig.guest_page(4u), 3u * page);

  /* From the middle of the first extent to the middle of the last: the walk
   * has to skip one extent's head and stop short of another's tail. */
  BlobDmaBuf middle(rig.host());
  CHECK_STATUS(middle.make(pages.window(), page, 3u * page), Status::ok);
  CHECK_EQ(middle.length(), 3u * page);
  /* Three items: the rest of the first extent, all of the second, the start
   * of the third. */
  CHECK_EQ(middle.items(), 3u);
  CHECK(middle.invariants_hold());
  middle.release();

  /* Exactly one extent, named by its own offset: one item and nothing else. */
  BlobDmaBuf single(rig.host());
  CHECK_STATUS(single.make(pages.window(), 2u * page, page), Status::ok);
  CHECK_EQ(single.items(), 1u);
  CHECK_EQ(single.length(), page);
  single.release();
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(blob_dma_buf, a_range_past_the_blob_is_refused_before_the_host)
{ Fixture rig;
  Extents pages(1u);
  pages.add(rig.guest_page(0u), 2u * page);
  BlobDmaBuf past(rig.host());
  CHECK_STATUS(past.make(pages.window(), 0u, 3u * page), Status::too_large);
  BlobDmaBuf beyond(rig.host());
  CHECK_STATUS(beyond.make(pages.window(), 2u * page, page), Status::too_large);
  /* And an offset that is not whole pages, which no item could start at. */
  BlobDmaBuf ragged(rig.host());
  CHECK_STATUS(ragged.make(pages.window(), 1u, page), Status::malformed);
  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  CHECK(past.invariants_hold());
  CHECK(beyond.invariants_hold());
  CHECK(ragged.invariants_hold()); }

/* A host blob is one block the host made: the dma-buf is the host's over the
 * range asked for, one item, and the descriptor is this object's to close. */
TEST(blob_dma_buf_make_over_host, the_host_makes_one_over_the_range_of_the_block)
{ Fixture rig;
  void       *view = nullptr;
  void *const memory = rig.host().create_host_memory(4u * page, view);
  CHECK(memory != nullptr);
  BlobDmaBuf buffer(rig.host());
  CHECK_STATUS(buffer.make_over_host(memory, 4u * page, page, 2u * page), Status::ok);
  CHECK(buffer.made());
  CHECK(buffer.handle() >= 0);
  CHECK_EQ(buffer.length(), 2u * page);
  CHECK_EQ(buffer.items(), 1u);
  CHECK(buffer.invariants_hold());
  CHECK_EQ(rig.host().live_dmabufs(), 1u);
  CHECK(rig.host().last_host_dmabuf().memory == memory);
  CHECK_EQ(rig.host().last_host_dmabuf().offset, page);
  CHECK_EQ(rig.host().last_host_dmabuf().size, 2u * page);
  const int held = buffer.handle();
  CHECK_STATUS(buffer.make_over_host(memory, 4u * page, 0u, page), Status::invalid_state);
  CHECK_EQ(buffer.handle(), held);
  CHECK_EQ(rig.host().host_dmabufs(), 1u);
  buffer.release();
  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  /* The last page exactly, and the whole block. */
  BlobDmaBuf last(rig.host());
  CHECK_STATUS(last.make_over_host(memory, 4u * page, 3u * page, page), Status::ok);
  BlobDmaBuf whole(rig.host());
  CHECK_STATUS(whole.make_over_host(memory, 4u * page, 0u, 4u * page), Status::ok);
  CHECK_EQ(rig.host().live_dmabufs(), 2u);
  last.release();
  whole.release();
  rig.host().destroy_host_memory(memory); }

TEST(blob_dma_buf_make_over_host, a_range_past_the_block_or_not_whole_pages_is_refused_before_the_host)
{ Fixture rig;
  void       *view = nullptr;
  void *const memory = rig.host().create_host_memory(2u * page, view);
  struct Case { std::uint64_t offset; std::uint64_t size; Status status; };
  const Case refused[] = {
      {0u, 3u * page, Status::too_large},
      {2u * page, page, Status::too_large},
      {page, 2u * page, Status::too_large},
      {0xFFFFFFFFFFFFF000u, 2u * page, Status::too_large},
      {0u, 0u, Status::malformed},
      {0u, page + 1u, Status::malformed},
      {1u, page, Status::malformed},
  };
  for (const Case &c : refused) {
      BlobDmaBuf buffer(rig.host());
      CHECK_STATUS(buffer.make_over_host(memory, 2u * page, c.offset, c.size), c.status);
      CHECK(!buffer.made());
      CHECK(buffer.invariants_hold());
  }
  CHECK_EQ(rig.host().host_dmabufs(), 0u);
  rig.host().destroy_host_memory(memory); }

TEST(blob_dma_buf_make_over_host, a_host_that_cannot_make_one_is_answered_and_nothing_is_held)
{ Fixture rig;
  void       *view = nullptr;
  void *const memory = rig.host().create_host_memory(page, view);
  BlobDmaBuf buffer(rig.host());
  rig.host().fail_dmabuf_at(1u);
  CHECK_STATUS(buffer.make_over_host(memory, page, 0u, page), Status::not_supported);
  CHECK(!buffer.made());
  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  CHECK(buffer.invariants_hold());
  CHECK_STATUS(buffer.make_over_host(memory, page, 0u, page), Status::ok);   /* the next is made */
  buffer.release();
  rig.host().destroy_host_memory(memory); }

TEST(blob_dma_buf_invariants, each_rule_is_detected)
{ Fixture rig;
  struct Case { int handle; std::uint64_t length; std::uint32_t items; };
  const Case broken[] = {
      {-1, page, 0u},                                  /* nothing held, but a length */
      {-1, 0u, 1u},                                    /* nothing held, but an item */
      {7, 0u, 1u},                                     /* held, with no length */
      {7, page, 0u},                                   /* held, with no item */
      {7, page, BlobDmaBuf::max_items + 1u},          /* more items than one may carry */
      {7, page + 8u, 1u},                              /* a length that is not whole pages */
  };
  for (const Case &c : broken) {
      BlobDmaBuf buffer(rig.host());
      av::BlobDmaBufProbe::set(buffer, c.handle, c.length, c.items);
      CHECK(!buffer.invariants_hold());
      av::BlobDmaBufProbe::set(buffer, -1, 0u, 0u);   /* nothing to close on the way out */
      CHECK(buffer.invariants_hold());
  } }
