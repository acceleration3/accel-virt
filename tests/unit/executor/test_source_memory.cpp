#include "device/abi/av_executor.h"
#include "executor/exported_memory.hpp"
#include "executor/owned_fd.hpp"
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

}

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

TEST(source_memory, answers_its_key_in_its_domain_until_detached)
{ SourceMemory memory(3u, 0x55u, 0u, AV_MEMORY_WRITE_BACK, ExportedMemory(1u, -1, mapping, sizeof mapping));
  CHECK(memory.answers(3u, 0x55u));
  CHECK(!memory.answers(4u, 0x55u));
  CHECK(!memory.answers(3u, 0x56u));
  memory.detach();
  CHECK(!memory.attached());
  CHECK(!memory.answers(3u, 0x55u));
  CHECK_EQ(memory.domain(), 3u);
  CHECK_EQ(memory.key(), 0x55u); }

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

TEST(source_memory, shares_a_descriptor_of_its_own_memory_that_the_caller_owns)
{ const int fd = ::memfd_create("source-memory", MFD_CLOEXEC);
  CHECK(fd >= 0);
  CHECK_EQ(::ftruncate(fd, 4096), 0);
  auto *const own = static_cast<std::uint8_t *>(::mmap(nullptr, 4096u, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  CHECK(own != MAP_FAILED);
  const SourceMemory memory(1u, 1u, 0u, AV_MEMORY_WRITE_BACK, ExportedMemory(1u, fd, own, 4096u));
  av::OwnedFd owned;
  CHECK(memory.share(owned));
  const int shared = owned.fd();
  CHECK(shared >= 0);
  CHECK(shared != fd);
  CHECK((::fcntl(shared, F_GETFD) & FD_CLOEXEC) != 0);
  const std::uint8_t byte = 0xA5u;
  CHECK_EQ(::pwrite(shared, &byte, 1u, 100), 1);
  CHECK_EQ(memory.pointer()[100], 0xA5u);
  owned.close();
  CHECK_EQ(::fcntl(shared, F_GETFD), -1);
  CHECK(::fcntl(fd, F_GETFD) != -1);
  ::munmap(own, 4096u);
  ::close(fd); }

TEST(source_memory, memory_with_no_descriptor_shares_none)
{ const SourceMemory memory(1u, 1u, 0u, AV_MEMORY_WRITE_BACK, ExportedMemory(1u, -1, mapping, sizeof mapping));
  av::OwnedFd owned;
  CHECK(!memory.share(owned));
  CHECK(!owned.held()); }
