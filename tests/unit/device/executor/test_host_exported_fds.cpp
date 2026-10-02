/* The fds this context exported from a host device's objects, and what an
 * import may name (feature 047, US3, contracts/external_objects.md). The
 * guest's ICD turns a Win32 handle into an import by asking for a fresh fd of
 * the object it names, so every fd an import carries is one this table handed
 * out; an fd the guest made up names something else in this process entirely,
 * and is refused rather than imported. */
#include "device/executor/host_exported_fds.hpp"
#include "harness/framework/test_registry.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>

namespace av {

/* The state no export can leave the table in. */
struct HostExportedFdsProbe {
    static void a_fd_of_no_device(HostExportedFds &table) { table.slots_[0].device_ = VK_NULL_HANDLE; }
    static void the_same_fd_twice(HostExportedFds &table) { table.slots_[1] = table.slots_[0]; }
    static void a_device_with_no_fd(HostExportedFds &table)
    {
        for (HostExportedFdSlot &slot : table.slots_)
            if (slot.fd_ < 0) {
                slot.device_ = reinterpret_cast<VkDevice>(0x9010u);
                return;
            }
    }
};

}  // namespace av

using av::HostExportedFds;
using av::HostExportedFdsProbe;

namespace {

VkDevice const device = reinterpret_cast<VkDevice>(0x9010u);
VkDevice const other_device = reinterpret_cast<VkDevice>(0x9020u);

/* An fd of this process, standing for one a host driver exported: what the
 * table holds is an fd, and what it does with one it still has is close it. */
int an_fd()
{
    return ::open("/dev/null", O_RDONLY | O_CLOEXEC);
}

bool open_still(int fd)
{
    return ::fcntl(fd, F_GETFD) >= 0;
}

}  // namespace

TEST(host_exported_fds, an_import_names_an_fd_this_context_exported)
{ HostExportedFds table;
  CHECK(table.invariants_hold());
  CHECK_EQ(table.live(), 0u);
  const int fd = an_fd();
  CHECK(fd >= 0);
  CHECK(table.keep(device, fd));
  CHECK(table.invariants_hold());
  CHECK_EQ(table.live(), 1u);
  /* An fd nobody exported is not an import's to name, and naming one does not
   * take the one that was exported. */
  CHECK(table.holds(fd));
  CHECK(!table.holds(fd + 1));
  CHECK(!table.holds(-1));
  CHECK(!table.take(fd + 1));
  CHECK(!table.take(-1));
  CHECK_EQ(table.live(), 1u);
  /* Claimed, it is the import's: the driver it goes to owns it, so the table
   * neither closes it nor answers it twice. */
  CHECK(table.take(fd));
  CHECK_EQ(table.live(), 0u);
  CHECK(!table.holds(fd));
  CHECK(!table.take(fd));
  CHECK(open_still(fd));
  CHECK(table.invariants_hold());
  ::close(fd); }

/* A table dying closes what it still holds, because a context dying is the one
 * way an export outlives the guest that asked for it. */
TEST(host_exported_fds, a_table_dying_closes_what_it_holds)
{ const int fd = an_fd();
  CHECK(fd >= 0);
  { HostExportedFds table;
    CHECK(table.keep(device, fd)); }
  CHECK(!open_still(fd)); }

/* A device destroyed takes its exports with it, and closes them: an fd the
 * guest asked for and never imported is one this process would hold until it
 * exited, which is a handle leak a guest could drive. */
TEST(host_exported_fds, a_device_closes_the_fds_it_exported)
{ HostExportedFds table;
  const int       kept = an_fd();
  const int       other = an_fd();
  CHECK(kept >= 0 && other >= 0);
  CHECK(table.keep(device, kept));
  CHECK(table.keep(other_device, other));
  CHECK_EQ(table.live(), 2u);
  table.forget_device(device);
  CHECK_EQ(table.live(), 1u);
  CHECK(!open_still(kept));
  CHECK(open_still(other));
  /* Forgetting a device that exported nothing, and the null one, is no-one's
   * error. */
  table.forget_device(device);
  table.forget_device(VK_NULL_HANDLE);
  CHECK_EQ(table.live(), 1u);
  CHECK(table.invariants_hold());
  table.forget_device(other_device);
  CHECK(!open_still(other));
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold()); }

/* A closing context forgets every fd, whichever of its devices exported it:
 * the next context in its slot must not be able to name one. */
TEST(host_exported_fds, forgetting_all_closes_every_device_s_fds)
{ HostExportedFds table;
  const int       kept = an_fd();
  const int       other = an_fd();
  CHECK(kept >= 0 && other >= 0);
  CHECK(table.keep(device, kept));
  CHECK(table.keep(other_device, other));
  table.forget_all();
  CHECK_EQ(table.live(), 0u);
  CHECK(!table.holds(kept));
  CHECK(!table.holds(other));
  CHECK(!open_still(kept));
  CHECK(!open_still(other));
  CHECK(table.invariants_hold());
  /* Emptied, it keeps again. */
  const int again = an_fd();
  CHECK(table.keep(device, again));
  CHECK(table.holds(again));
  table.forget_all();
  CHECK(!open_still(again)); }

/* Nothing to keep, and nowhere to keep it: a refused export is the caller's
 * fd to close, which is why it is told rather than left to assume. */
TEST(host_exported_fds, an_export_with_nowhere_to_go_is_refused)
{ HostExportedFds table;
  CHECK(!table.keep(device, -1));
  const int nowhere = an_fd();
  CHECK(nowhere >= 0);
  CHECK(!table.keep(VK_NULL_HANDLE, nowhere));
  CHECK(open_still(nowhere));
  ::close(nowhere);
  CHECK_EQ(table.live(), 0u);
  int held[HostExportedFds::max_fds] = {};
  for (std::uint32_t index = 0u; index < HostExportedFds::max_fds; index++) {
      held[index] = an_fd();
      CHECK(held[index] >= 0);
      CHECK(table.keep(device, held[index]));
  }
  CHECK_EQ(table.live(), HostExportedFds::max_fds);
  const int spare = an_fd();
  CHECK(spare >= 0);
  CHECK(!table.keep(device, spare));
  CHECK_EQ(table.live(), HostExportedFds::max_fds);
  CHECK(open_still(spare));
  ::close(spare);
  CHECK(table.invariants_hold());
  table.forget_device(device);
  CHECK_EQ(table.live(), 0u); }

/* The states the table's own bookkeeping must never be in. */
TEST(host_exported_fds, an_impossible_table_is_named)
{ HostExportedFds table;
  const int       fd = an_fd();
  CHECK(fd >= 0);
  CHECK(table.keep(device, fd));
  HostExportedFdsProbe::a_fd_of_no_device(table);
  CHECK(!table.invariants_hold());

  HostExportedFds second;
  const int       kept = an_fd();
  CHECK(kept >= 0);
  CHECK(second.keep(device, kept));
  HostExportedFdsProbe::the_same_fd_twice(second);
  CHECK(!second.invariants_hold());

  HostExportedFds third;
  HostExportedFdsProbe::a_device_with_no_fd(third);
  CHECK(!third.invariants_hold()); }
