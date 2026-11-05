#include "executor/owned_fd.hpp"
#include "harness/framework/test_registry.hpp"

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

using av::OwnedFd;

namespace {

bool open_descriptor(int fd) { return fd >= 0 && fcntl(fd, F_GETFD) != -1; }

}

TEST(owned_fd, closes_the_descriptor_it_holds_when_it_goes)
{ int fd = eventfd(0u, EFD_CLOEXEC);
  CHECK(fd >= 0);
  {
      OwnedFd owner(fd);
      CHECK(owner.held());
      CHECK_EQ(owner.fd(), fd);
      CHECK(open_descriptor(fd));
  }
  CHECK(!open_descriptor(fd)); }

TEST(owned_fd, holds_nothing_until_given_a_descriptor_and_lets_go_when_taken)
{ OwnedFd owner;
  CHECK(!owner.held());
  CHECK_EQ(owner.fd(), -1);
  const int fd = eventfd(0u, EFD_CLOEXEC);
  owner.hold(fd);
  CHECK(owner.held());
  CHECK_EQ(owner.take(), fd);
  CHECK(!owner.held());
  CHECK(open_descriptor(fd));
  owner.hold(fd);
  owner.close();
  CHECK(!owner.held());
  CHECK(!open_descriptor(fd)); }

TEST(owned_fd_hold_copy, holds_a_duplicate_and_leaves_the_original_to_its_owner)
{ const int fd = eventfd(0u, EFD_CLOEXEC);
  OwnedFd   copy;
  CHECK(copy.hold_copy(fd));
  CHECK(copy.held());
  CHECK(copy.fd() != fd);
  CHECK(open_descriptor(copy.fd()));
  CHECK((fcntl(copy.fd(), F_GETFD) & FD_CLOEXEC) != 0);
  close(fd);
  CHECK(open_descriptor(copy.fd()));
  const int first = copy.fd();
  CHECK(!copy.hold_copy(-1));
  CHECK(!copy.held());
  CHECK(!open_descriptor(first)); }

TEST(owned_fd_copy_to, duplicates_into_another_owner_and_a_held_nothing_copies_nothing)
{ OwnedFd original(eventfd(0u, EFD_CLOEXEC));
  OwnedFd other;
  CHECK(original.copy_to(other));
  CHECK(other.held());
  CHECK(other.fd() != original.fd());
  const int kept = other.fd();
  OwnedFd empty;
  CHECK(!empty.copy_to(other));
  CHECK(!other.held());
  CHECK(!open_descriptor(kept));
  CHECK(original.held()); }
