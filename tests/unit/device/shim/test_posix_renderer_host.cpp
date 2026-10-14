/* The renderer process's host, for real: its control thread and the
 * routines posted to it, workers woken by their own eventfd or by the
 * doorbell QEMU registered, guest RAM mapped from the memfds QEMU sent,
 * host blobs over the memfd QEMU placed, fence interrupts written to the
 * notifiers QEMU passed, recordings on disk - and everything only QEMU has
 * answered not_supported. */
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/display/scanout_framebuffer.hpp"
#include "device/gpu/pci_address.hpp"
#include "device/renderer/doorbell_offers.hpp"
#include "device/renderer/forwarded_request.hpp"
#include "device/renderer/guest_memory_table.hpp"
#include "device/shim/posix_renderer_host.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/gpu_identity.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <unistd.h>

using av::DoorbellOffers;
using av::GuestMemoryTable;
using av::PosixRendererHost;
using av::Status;

namespace {

constexpr std::uint64_t page = 4096u;

class Rig {
public:
    Rig() : host_(memory_, offers_) {}
    PosixRendererHost &host() { return host_; }
    GuestMemoryTable  &memory() { return memory_; }
    DoorbellOffers    &offers() { return offers_; }

private:
    GuestMemoryTable  memory_;
    DoorbellOffers    offers_;
    PosixRendererHost host_;
};

int memfd(std::uint64_t size)
{
    const int descriptor = ::memfd_create("av-test", MFD_CLOEXEC);
    if (descriptor >= 0 && ::ftruncate(descriptor, static_cast<off_t>(size)) != 0) {
        ::close(descriptor);
        return -1;
    }
    return descriptor;
}

bool readable(int descriptor)
{
    pollfd watched = {descriptor, POLLIN, 0};
    return ::poll(&watched, 1u, 0) == 1 && (watched.revents & POLLIN) != 0;
}

bool is_open(int descriptor) { return ::fcntl(descriptor, F_GETFD) != -1; }

std::uint64_t read_counter(int descriptor)
{
    std::uint64_t value = 0u;
    if (::read(descriptor, &value, sizeof value) != static_cast<ssize_t>(sizeof value)) return 0u;
    return value;
}

void post_one(int descriptor)
{
    const std::uint64_t one = 1u;
    static_cast<void>(::write(descriptor, &one, sizeof one));
}

/* What the process writes to stderr while `run` runs. */
class StderrCapture {
public:
    StderrCapture() : saved_(::dup(2)), ends_{-1, -1}, text_{}
    {
        static_cast<void>(::pipe2(ends_, O_CLOEXEC | O_NONBLOCK));
        std::fflush(stderr);
        ::dup2(ends_[1], 2);
    }
    ~StderrCapture()
    {
        finish();
        ::close(ends_[0]);
    }
    const char *finish()
    {
        if (saved_ < 0) return text_;
        std::fflush(stderr);
        ::dup2(saved_, 2);
        ::close(saved_);
        ::close(ends_[1]);
        saved_ = -1;
        const ssize_t got = ::read(ends_[0], text_, sizeof text_ - 1u);
        text_[got > 0 ? got : 0] = '\0';
        return text_;
    }

private:
    int  saved_;
    int  ends_[2];
    char text_[1024];
};

/* What a posted routine writes: its number, in the order routines ran. */
struct Ledger {
    std::uint32_t ran[8];
    std::uint32_t count;
};

struct Entry {
    Ledger       *ledger;
    std::uint32_t number;
};

void note(void *context)
{
    Entry *const entry = static_cast<Entry *>(context);
    entry->ledger->ran[entry->ledger->count++] = entry->number;
}

struct Poster {
    PosixRendererHost *host;
    Entry             *entries;
    std::uint32_t      count;
};

void post_all(void *context)
{
    Poster *const poster = static_cast<Poster *>(context);
    for (std::uint32_t index = 0u; index < poster->count; index++) poster->host->run_on_main_loop(&note, &poster->entries[index]);
}

struct Asker {
    PosixRendererHost *host;
};

void ask_main_loop(void *context)
{
    static_cast<Asker *>(context)->host->require_main_loop("Asker::ask");
}

struct Waiter {
    PosixRendererHost *host;
    void              *event;
    std::atomic<bool>  woke;
};

void wait_once(void *context)
{
    Waiter *const waiter = static_cast<Waiter *>(context);
    waiter->host->wait_event(waiter->event);
    waiter->woke = true;
}

struct Locker {
    PosixRendererHost *host;
    void              *mutex;
    bool               took;
};

void try_elsewhere(void *context)
{
    Locker *const locker = static_cast<Locker *>(context);
    locker->took = locker->host->try_lock_mutex(locker->mutex);
    if (locker->took) locker->host->unlock_mutex(locker->mutex);
}

}  // namespace

/* Routines posted from any thread run on the control thread, in the order
 * posted, when it runs them; the wakeup is readable exactly while some wait. */
TEST(posix_renderer_host_main_loop, runs_posted_routines_in_order_when_the_control_thread_asks)
{ Rig rig;
  PosixRendererHost &host = rig.host();
  CHECK_EQ(host.main_loop_wakeup(), -1);
  Ledger ledger = {};
  Entry  early = {&ledger, 0u};
  host.run_on_main_loop(&note, &early);   /* before there is a wakeup to signal */
  CHECK_STATUS(host.become_main_loop(), Status::ok);
  const int wakeup = host.main_loop_wakeup();
  CHECK(wakeup >= 0);
  CHECK(!readable(wakeup));
  Entry  entries[3] = {{&ledger, 1u}, {&ledger, 2u}, {&ledger, 3u}};
  Poster poster = {&host, entries, 3u};
  void  *thread = nullptr;
  CHECK_STATUS(host.start_thread(&post_all, &poster, thread), Status::ok);
  host.join_thread(thread);
  CHECK(readable(wakeup));
  CHECK_EQ(ledger.count, 0u);   /* nothing runs until the control thread runs it */
  host.run_main_loop_work();
  CHECK_EQ(ledger.count, 4u);
  CHECK_EQ(ledger.ran[0], 0u);
  CHECK_EQ(ledger.ran[1], 1u);
  CHECK_EQ(ledger.ran[2], 2u);
  CHECK_EQ(ledger.ran[3], 3u);
  CHECK(!readable(wakeup));
  host.run_main_loop_work();
  CHECK_EQ(ledger.count, 4u);
  /* A second become keeps the wakeup it made. */
  CHECK_STATUS(host.become_main_loop(), Status::ok);
  CHECK_EQ(host.main_loop_wakeup(), wakeup); }

/* Routines never run are freed with the host: LeakSanitizer is the judge. */
TEST(posix_renderer_host_main_loop, routines_never_run_go_with_the_host)
{ Ledger ledger = {};
  Entry  entries[2] = {{&ledger, 1u}, {&ledger, 2u}};
  { Rig rig;
    CHECK_STATUS(rig.host().become_main_loop(), Status::ok);
    rig.host().run_on_main_loop(&note, &entries[0]);
    rig.host().run_on_main_loop(&note, &entries[1]); }
  CHECK_EQ(ledger.count, 0u); }

/* The thread checks only complain; they complain about the right thread. */
TEST(posix_renderer_host_main_loop, names_a_call_on_the_wrong_thread)
{ Rig rig;
  PosixRendererHost &host = rig.host();
  { StderrCapture capture;
    host.require_main_loop("Before::become");
    CHECK(std::strstr(capture.finish(), "av-renderer: Before::become called off the control thread") != nullptr); }
  CHECK_STATUS(host.become_main_loop(), Status::ok);
  { StderrCapture capture;
    host.require_main_loop("Main::ok");
    host.require_worker("Worker::here");
    const char *const text = capture.finish();
    CHECK(std::strstr(text, "Main::ok") == nullptr);
    CHECK(std::strstr(text, "av-renderer: Worker::here called on the control thread") != nullptr); }
  { StderrCapture capture;
    Asker  asker = {&host};
    void  *thread = nullptr;
    CHECK_STATUS(host.start_thread(&ask_main_loop, &asker, thread), Status::ok);
    host.join_thread(thread);
    CHECK(std::strstr(capture.finish(), "av-renderer: Asker::ask called off the control thread") != nullptr); }
  { StderrCapture capture;
    host.log("hello there");
    CHECK(std::strcmp(capture.finish(), "av-renderer: hello there\n") == 0); } }

/* A worker's event wakes on its own signal, auto-resetting. */
TEST(posix_renderer_host_event, wakes_on_its_own_signal_and_resets)
{ Rig rig;
  PosixRendererHost &host = rig.host();
  void *event = nullptr;
  CHECK_STATUS(host.create_event(event), Status::ok);
  const int own = host.event_descriptor(event);
  CHECK(own >= 0);
  host.signal_event(event);
  host.signal_event(event);
  CHECK(readable(own));
  host.wait_event(event);
  CHECK(!readable(own));
  Waiter waiter = {&host, event, {false}};
  void  *thread = nullptr;
  CHECK_STATUS(host.start_thread(&wait_once, &waiter, thread), Status::ok);
  host.signal_event(event);
  host.join_thread(thread);
  CHECK(waiter.woke.load());
  host.destroy_event(event); }

/* The doorbell QEMU offered for the page joins the worker's wakeup: a guest
 * store - a write to QEMU's eventfd - wakes it, and both are drained. */
TEST(posix_renderer_host_event, wakes_on_the_doorbell_qemu_offered)
{ Rig rig;
  PosixRendererHost &host = rig.host();
  void *event = nullptr;
  CHECK_STATUS(host.create_event(event), Status::ok);
  host.connect_doorbell(event, 4u);   /* nothing offered: nothing joins */
  const int kvm_side = ::eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
  const int offered = ::fcntl(kvm_side, F_DUPFD_CLOEXEC, 0);
  CHECK_STATUS(rig.offers().offer(4u, offered), Status::ok);
  host.connect_doorbell(event, 4u);
  int left = -1;
  CHECK_STATUS(rig.offers().take(4u, left), Status::not_found);
  post_one(kvm_side);
  Waiter waiter = {&host, event, {false}};
  void  *thread = nullptr;
  CHECK_STATUS(host.start_thread(&wait_once, &waiter, thread), Status::ok);
  host.join_thread(thread);
  CHECK(waiter.woke.load());
  CHECK(!readable(kvm_side));
  /* Both at once: both drained. */
  post_one(kvm_side);
  host.signal_event(event);
  host.wait_event(event);
  CHECK(!readable(kvm_side));
  CHECK(!readable(host.event_descriptor(event)));
  /* A new offer at the page replaces the old descriptor, which is closed. */
  const int replacement = ::fcntl(kvm_side, F_DUPFD_CLOEXEC, 0);
  CHECK_STATUS(rig.offers().offer(4u, replacement), Status::ok);
  host.connect_doorbell(event, 4u);
  CHECK(!is_open(offered));
  host.disconnect_doorbell(event, 4u);
  CHECK(!is_open(replacement));
  host.disconnect_doorbell(event, 4u);   /* twice is nothing */
  post_one(kvm_side);
  host.signal_event(event);
  host.wait_event(event);
  CHECK(readable(kvm_side));   /* no longer drained: no longer joined */
  ::close(kvm_side);
  host.destroy_event(event); }

/* The event goes with the doorbell it holds. */
TEST(posix_renderer_host_event, destroying_an_event_closes_its_doorbell)
{ Rig rig;
  void *event = nullptr;
  CHECK_STATUS(rig.host().create_event(event), Status::ok);
  const int offered = ::eventfd(0u, EFD_CLOEXEC);
  CHECK_STATUS(rig.offers().offer(0u, offered), Status::ok);
  rig.host().connect_doorbell(event, 0u);
  rig.host().destroy_event(event);
  CHECK(!is_open(offered)); }

TEST(posix_renderer_host_mutex, excludes_another_thread_until_unlocked)
{ Rig rig;
  PosixRendererHost &host = rig.host();
  void *mutex = nullptr;
  CHECK_STATUS(host.create_mutex(mutex), Status::ok);
  host.lock_mutex(mutex);
  Locker locker = {&host, mutex, true};
  void  *thread = nullptr;
  CHECK_STATUS(host.start_thread(&try_elsewhere, &locker, thread), Status::ok);
  host.join_thread(thread);
  CHECK(!locker.took);
  host.unlock_mutex(mutex);
  CHECK_STATUS(host.start_thread(&try_elsewhere, &locker, thread), Status::ok);
  host.join_thread(thread);
  CHECK(locker.took);
  CHECK(host.try_lock_mutex(mutex));
  host.unlock_mutex(mutex);
  host.destroy_mutex(mutex); }

/* A region is QEMU's memfd mapped shared: what one side writes the other
 * reads. One that does not map is no_memory. */
TEST(posix_renderer_host_region, maps_qemus_memfd_shared_and_refuses_what_does_not_map)
{ Rig rig;
  const int region = memfd(4u * page);
  void     *base = nullptr;
  CHECK_STATUS(rig.host().map_region(region, 4u * page, base), Status::ok);
  const std::uint8_t byte = 0xA7u;
  CHECK_EQ(::pwrite(region, &byte, 1u, 0x123), 1);
  CHECK_EQ(static_cast<std::uint8_t *>(base)[0x123], 0xA7u);
  rig.host().unmap_region(base, 4u * page);
  void *none = &base;
  CHECK_STATUS(rig.host().map_region(-1, page, none), Status::no_memory);
  CHECK(none == nullptr);
  rig.host().close_descriptor(region);
  CHECK(!is_open(region));
  rig.host().close_descriptor(-1); }

/* Guest memory is what the regions hold: an extent resolves to the mapped
 * bytes, directly, and one outside every region to nothing. */
TEST(posix_renderer_host_guest, maps_extents_and_aliases_runs_through_the_regions)
{ Rig rig;
  PosixRendererHost &host = rig.host();
  const int region = memfd(4u * page);
  void     *base = nullptr;
  CHECK_STATUS(host.map_region(region, 4u * page, base), Status::ok);
  CHECK_STATUS(rig.memory().add(2u, base, 4u * page, region), Status::ok);
  std::uint64_t length = page;
  bool          direct = false;
  void *const   mapped = host.map_guest(std::uint64_t{2u} << GuestMemoryTable::region_shift | page, length, direct);
  CHECK(mapped == static_cast<std::uint8_t *>(base) + page);
  CHECK(direct);
  CHECK_EQ(length, page);
  host.unmap_guest(mapped, length);
  length = 2u * page;
  CHECK(host.map_guest(std::uint64_t{2u} << GuestMemoryTable::region_shift | 3u * page, length, direct) == nullptr);
  CHECK(!direct);
  CHECK_EQ(length, 0u);
  length = page;
  CHECK(host.map_guest(std::uint64_t{5u} << GuestMemoryTable::region_shift, length, direct) == nullptr);
  /* A run aliased at a reserved range is the same pages. */
  void *range = nullptr;
  CHECK_STATUS(host.reserve_range(2u * page, range), Status::ok);
  CHECK_STATUS(host.alias_guest_run(static_cast<std::uint8_t *>(base) + 2u * page, page, range), Status::ok);
  static_cast<std::uint8_t *>(base)[2u * page + 5u] = 0x3Eu;
  CHECK_EQ(static_cast<std::uint8_t *>(range)[5], 0x3Eu);
  static_cast<std::uint8_t *>(range)[6] = 0x4Fu;
  CHECK_EQ(static_cast<std::uint8_t *>(base)[2u * page + 6u], 0x4Fu);
  std::uint8_t outside[page] = {};
  CHECK_STATUS(host.alias_guest_run(outside, page, range), Status::not_supported);
  host.release_range(range, 2u * page);
  int           removed = -1;
  std::uint64_t size = 0u;
  CHECK_STATUS(rig.memory().remove(2u, base, size, removed), Status::ok);
  host.unmap_region(base, size);
  host.close_descriptor(removed); }

/* A fence interrupt is a write to the guest notifier of its line, while the
 * lines are connected and only for a line QEMU gave. */
TEST(posix_renderer_host_interrupts, raising_a_line_writes_its_notifier_while_connected)
{ Rig rig;
  PosixRendererHost &host = rig.host();
  const int line0 = ::eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
  const int line2 = ::eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
  const int given[3] = {::fcntl(line0, F_DUPFD_CLOEXEC, 0), -1, ::fcntl(line2, F_DUPFD_CLOEXEC, 0)};
  host.take_interrupt_lines(given, 3u);
  host.raise_fence_interrupt(2u);   /* not connected */
  CHECK(!readable(line2));
  CHECK_STATUS(host.connect_fence_interrupts(), Status::ok);
  host.raise_fence_interrupt(2u);
  host.raise_fence_interrupt(2u);
  host.raise_fence_interrupt(1u);   /* no notifier */
  host.raise_fence_interrupt(PosixRendererHost::max_interrupt_lines);
  CHECK_EQ(read_counter(line2), 2u);
  CHECK(!readable(line0));
  host.raise_fence_interrupt(0u);
  CHECK_EQ(read_counter(line0), 1u);
  host.disconnect_fence_interrupts();
  host.raise_fence_interrupt(0u);
  CHECK(!readable(line0));
  /* New lines replace the old, which are closed. */
  const int replacement[1] = {::fcntl(line0, F_DUPFD_CLOEXEC, 0)};
  host.take_interrupt_lines(replacement, 1u);
  CHECK(!is_open(given[0]));
  CHECK(!is_open(given[2]));
  host.drop_interrupt_lines();
  CHECK(!is_open(replacement[0]));
  ::close(line0);
  ::close(line2); }

/* More lines than there are doorbell pages: those past the last are closed
 * at once. */
TEST(posix_renderer_host_interrupts, lines_past_the_last_page_are_closed)
{ Rig rig;
  constexpr std::uint32_t count = PosixRendererHost::max_interrupt_lines + 2u;
  const int source = ::eventfd(0u, EFD_CLOEXEC);
  int       given[count];
  for (int &line : given) line = ::fcntl(source, F_DUPFD_CLOEXEC, 0);
  given[count - 1u] = -1;
  rig.host().take_interrupt_lines(given, count);
  CHECK(is_open(given[PosixRendererHost::max_interrupt_lines - 1u]));
  CHECK(!is_open(given[PosixRendererHost::max_interrupt_lines]));
  rig.host().drop_interrupt_lines();
  CHECK(!is_open(given[0]));
  ::close(source); }

/* A recording is a file: appended to, truncated back to nothing, closed. */
TEST(posix_renderer_host_recording, writes_truncates_and_closes_a_file)
{ Rig rig;
  PosixRendererHost &host = rig.host();
  char directory[] = "/tmp/av-renderer-host-XXXXXX";
  CHECK(::mkdtemp(directory) != nullptr);
  char path[128] = {};
  std::snprintf(path, sizeof path, "%s/recording", directory);
  void *file = nullptr;
  CHECK_STATUS(host.create_recording(path, file), Status::ok);
  const std::uint8_t first[4] = {1u, 2u, 3u, 4u};
  CHECK_STATUS(host.append_recording(file, first, sizeof first), Status::ok);
  CHECK_STATUS(host.truncate_recording(file), Status::ok);
  const std::uint8_t second[2] = {9u, 8u};
  CHECK_STATUS(host.append_recording(file, second, sizeof second), Status::ok);
  host.close_recording(file);
  char        bytes[16] = {};
  std::size_t length = 0u;
  CHECK_STATUS(host.read_file(path, bytes, sizeof bytes, length), Status::ok);
  CHECK_EQ(length, 2u);
  CHECK_EQ(static_cast<std::uint8_t>(bytes[0]), 9u);
  CHECK_EQ(static_cast<std::uint8_t>(bytes[1]), 8u);
  char missing[160] = {};
  std::snprintf(missing, sizeof missing, "%s/no/such/dir/recording", directory);
  CHECK_STATUS(host.create_recording(missing, file), Status::not_found);
  CHECK(file == nullptr);
  CHECK_STATUS(host.read_file(missing, bytes, sizeof bytes, length), Status::not_found);
  CHECK_EQ(length, 0u);
  ::unlink(path);
  ::rmdir(directory); }

/* The requests the renderer's blob table reads are the bytes QEMU forwarded. */
TEST(posix_renderer_host, reads_forwarded_requests_and_answers_none)
{ Rig rig;
  const std::uint8_t    bytes[6] = {1u, 2u, 3u, 4u, 5u, 6u};
  av::ForwardedRequest request(bytes, sizeof bytes);
  std::uint8_t          read[8] = {};
  CHECK_EQ(rig.host().read_request(&request, 2u, read, sizeof read), 4u);
  CHECK_EQ(read[0], 3u);
  CHECK_EQ(read[3], 6u);
  CHECK_EQ(rig.host().read_request(&request, 6u, read, sizeof read), 0u);
  rig.host().respond(&request, bytes, sizeof bytes);
  CHECK(!rig.host().resource_exists(1u)); }

/* What only QEMU has - the GPU catalog, the hostmem BAR, the display, guest
 * RAM blocks, the renderer itself - the renderer does not pretend to. */
TEST(posix_renderer_host, what_only_qemu_has_is_not_supported)
{ Rig rig;
  PosixRendererHost &host = rig.host();
  CHECK_EQ(host.gpu_count(), 0u);
  CHECK_EQ(host.gpu_identity(0u).device_type(), av::GpuIdentity().device_type());
  av::PciAddress address;
  CHECK_STATUS(host.gpu_pci_address(0u, address), Status::not_supported);
  CHECK_EQ(host.hostmem_size(), 0u);
  const int placed = memfd(page);
  CHECK_STATUS(host.place_in_hostmem(placed, page, 0u), Status::not_supported);
  CHECK(is_open(placed));   /* the caller's, untouched */
  ::close(placed);
  host.remove_from_hostmem(0u, page);
  std::uint8_t buffer[8] = {};
  CHECK_STATUS(host.bind_gpu(0u), Status::not_supported);
  CHECK(!host.display_takes_dmabuf(0u));
  CHECK_STATUS(host.show_scanout(0u, buffer, av::ScanoutFramebuffer()), Status::not_supported);
  host.update_scanout(0u, 0u, 0u, 1u, 1u);
  host.hide_scanout(0u);
  CHECK_EQ(host.fence_interrupt_descriptor(0u), -1);
  CHECK_EQ(host.guest_region_count(), 0u);
  std::uint32_t region = 9u;
  int           descriptor = 9;
  std::uint64_t size = 0u;
  CHECK_STATUS(host.guest_region(0u, region, descriptor, size), Status::not_supported);
  CHECK_EQ(descriptor, -1);
  std::uint64_t offset = 0u;
  CHECK_STATUS(host.locate_guest(buffer, 1u, region, offset), Status::not_found);
  CHECK_STATUS(host.start_renderer("/bin/true"), Status::not_supported);
  CHECK_STATUS(host.send_renderer(buffer, 1u, nullptr, 0u), Status::not_supported);
  std::size_t   length = 5u;
  std::uint32_t count = 5u;
  CHECK_STATUS(host.receive_renderer(buffer, sizeof buffer, length, nullptr, 0u, count, 0u), Status::not_supported);
  CHECK_EQ(length, 0u);
  CHECK_EQ(count, 0u);
  host.kill_renderer(); }

/* The executor is the library QEMU named; a path that does not open, is
 * empty or does not fit is not_found, and a library already loaded is not
 * loaded again. */
TEST(posix_renderer_host_executor, loads_the_library_the_hello_named)
{ Rig rig;
  const AvExecutorOps *ops = reinterpret_cast<const AvExecutorOps *>(&rig);
  CHECK_STATUS(rig.host().load_executor("/no/such/library.so", 19u, ops), Status::not_found);
  CHECK(ops == nullptr);
  CHECK_STATUS(rig.host().load_executor(AV_ECHO_EXECUTOR, 0u, ops), Status::not_found);
  CHECK_STATUS(rig.host().load_executor(nullptr, 4u, ops), Status::not_found);
  static char longest[4096];
  std::memset(longest, 'a', sizeof longest);
  CHECK_STATUS(rig.host().load_executor(longest, sizeof longest, ops), Status::not_found);
  /* The path is `length` bytes, not a C string: what follows is not read. */
  char padded[sizeof AV_ECHO_EXECUTOR + 4u] = {};
  std::memcpy(padded, AV_ECHO_EXECUTOR, sizeof AV_ECHO_EXECUTOR - 1u);
  std::memcpy(padded + sizeof AV_ECHO_EXECUTOR - 1u, "xyz", 3u);
  CHECK_STATUS(rig.host().load_executor(padded, sizeof AV_ECHO_EXECUTOR - 1u, ops), Status::ok);
  CHECK(ops != nullptr);
  CHECK(ops != nullptr && (ops->sub_apis(ops->opaque) & AV_EXECUTOR_SUB_API_VULKAN) != 0u);
  CHECK_STATUS(rig.host().load_executor(padded, sizeof AV_ECHO_EXECUTOR - 1u, ops), Status::invalid_state); }

TEST(posix_renderer_host, its_clock_moves_forward_and_its_memory_is_zeroed)
{ Rig rig;
  const std::uint64_t before = rig.host().monotonic_nanoseconds();
  ::usleep(1000u);
  const std::uint64_t after = rig.host().monotonic_nanoseconds();
  CHECK(after >= before + 1000000u);
  auto *const block = static_cast<std::uint8_t *>(rig.host().allocate(64u));
  CHECK(block != nullptr);
  bool zeroed = true;
  for (std::uint32_t index = 0u; block != nullptr && index < 64u; index++) zeroed = zeroed && block[index] == 0u;
  CHECK(zeroed);
  rig.host().release(block); }
