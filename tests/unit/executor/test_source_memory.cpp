/* One allocation of a GPU's memory source, counted: the device's blob holds
 * one reference and every import one more. What a caller acts on is which
 * key it still answers to, whether a release was the last, and the
 * descriptor a host API imports it by - a duplicate the caller owns. */
#include "device/abi/av_executor.h"
#include "executor/exported_memory.hpp"
#include "executor/source_memory.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

using av::ExportedMemory;
using av::SourceMemory;

namespace {

std::uint8_t mapping[4096];

}  // namespace

TEST(source_memory, answers_what_it_was_allocated_as)
{ const SourceMemory memory(3u, 0x55u, 7u, AV_MEMORY_WRITE_COMBINED,
                           ExportedMemory(0x1000u, -1, mapping, sizeof mapping));
  CHECK_EQ(memory.domain(), 3u);
  CHECK_EQ(memory.key(), 0x55u);
  CHECK_EQ(memory.memory_type(), 7u);
  CHECK_EQ(memory.caching(), std::uint32_t{AV_MEMORY_WRITE_COMBINED});
  CHECK_EQ(memory.size(), sizeof mapping);
  CHECK(memory.pointer() == mapping);
  CHECK_EQ(memory.exported().handle(), 0x1000u);
  CHECK(memory.attached());
  CHECK_EQ(memory.references(), 1u); }

/* It answers to its own key in its own domain only, and to nothing once the
 * device let the key go: the guest may reuse the key at once. */
TEST(source_memory, answers_its_key_in_its_domain_until_detached)
{ SourceMemory memory(3u, 0x55u, 0u, AV_MEMORY_WRITE_BACK, ExportedMemory(1u, -1, mapping, sizeof mapping));
  CHECK(memory.answers(3u, 0x55u));
  CHECK(!memory.answers(4u, 0x55u));
  CHECK(!memory.answers(3u, 0x56u));
  memory.detach();
  CHECK(!memory.attached());
  CHECK(!memory.answers(3u, 0x55u));
  /* What it is does not change with it. */
  CHECK_EQ(memory.domain(), 3u);
  CHECK_EQ(memory.key(), 0x55u); }

/* Only the last release says so: the device's hold and an import's are the
 * same kind of reference, in any order. */
TEST(source_memory, the_last_release_of_every_reference_is_the_one_that_says_so)
{ SourceMemory memory(1u, 1u, 0u, AV_MEMORY_WRITE_BACK, ExportedMemory(1u, -1, mapping, sizeof mapping));
  memory.acquire();
  memory.acquire();
  CHECK_EQ(memory.references(), 3u);
  CHECK(!memory.release());
  CHECK(!memory.release());
  CHECK_EQ(memory.references(), 1u);
  CHECK(memory.release());
  CHECK_EQ(memory.references(), 0u); }

/* What a host API imports is a new descriptor of the same dma-buf, the
 * caller's to close: closing it leaves the memory's own, and a byte written
 * through it is the memory's. A memfd stands in for the dma-buf. */
TEST(source_memory, shares_a_descriptor_of_its_own_memory_that_the_caller_owns)
{ const int fd = ::memfd_create("source-memory", MFD_CLOEXEC);
  CHECK(fd >= 0);
  CHECK_EQ(::ftruncate(fd, 4096), 0);
  auto *const own = static_cast<std::uint8_t *>(::mmap(nullptr, 4096u, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  CHECK(own != MAP_FAILED);
  const SourceMemory memory(1u, 1u, 0u, AV_MEMORY_WRITE_BACK, ExportedMemory(1u, fd, own, 4096u));
  const int shared = memory.share();
  CHECK(shared >= 0);
  CHECK(shared != fd);
  CHECK((::fcntl(shared, F_GETFD) & FD_CLOEXEC) != 0);
  const std::uint8_t byte = 0xA5u;
  CHECK_EQ(::pwrite(shared, &byte, 1u, 100), 1);
  CHECK_EQ(memory.pointer()[100], 0xA5u);
  ::close(shared);
  CHECK(::fcntl(fd, F_GETFD) != -1);
  ::munmap(own, 4096u);
  ::close(fd); }

/* Memory with no descriptor has none to share. */
TEST(source_memory, memory_with_no_descriptor_shares_none)
{ const SourceMemory memory(1u, 1u, 0u, AV_MEMORY_WRITE_BACK, ExportedMemory(1u, -1, mapping, sizeof mapping));
  CHECK_EQ(memory.share(), -1); }
