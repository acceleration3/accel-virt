/* The device module's trace services on this host, for real: the table in
 * /dev/shm, the state socket answered through the main-loop handshake, and
 * the crash file with its chaining. One DeviceTrace per process, so one test
 * walks every stage. */
#include "core/trace/scope_binding.hpp"
#include "core/trace/scope_name.hpp"
#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "core/trace/trace_table.hpp"
#include "device/shim/device_trace.hpp"
#include "device/shim/host_trace.hpp"
#include "device/shim/posix_module_resolver.hpp"
#include "device/shim/posix_trace_host.hpp"
#include "device/shim/shm_trace_region.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

using av::DeviceTraceRoot;
using av::HostTrace;
using av::ShmTraceRegion;
using av::Status;
using av::TraceSnapshot;

namespace {

/* How the test's main loop answers: at once, or never. */
bool main_loop_answers = true;
int  bus_handled = 0;

void schedule(RvkHostRoutine routine, void *context)
{
    if (main_loop_answers) routine(context);
}

void count_bus(int, siginfo_t *, void *) { bus_handled++; }

void forward_crash(int signal, siginfo_t *info, void *context);

class Root final : public av::StateReporter {
public:
    void report(av::StateWriter &writer) override
    {
        writer.open("module", 0u);
        writer.number("devices", 0u);
        writer.close();
    }
};

Root            root;
DeviceTraceRoot services;

void forward_crash(int signal, siginfo_t *info, void *context) { services.crashed(signal, info, context); }

void path_for(char *path, std::size_t size, const char *format)
{
    std::snprintf(path, size, format, static_cast<int>(getpid()));
}

/* A client's whole exchange with the state socket. */
std::size_t ask(const char *request, char *answer, std::size_t capacity)
{
    char path[108];
    path_for(path, sizeof path, "/dev/shm/rvk-state-device-%d.sock");
    const int client = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    std::strcpy(address.sun_path, path);
    CHECK(connect(client, static_cast<sockaddr *>(static_cast<void *>(&address)), sizeof address) == 0);
    CHECK(write(client, request, std::strlen(request)) == static_cast<ssize_t>(std::strlen(request)));
    std::size_t length = 0u;
    for (ssize_t got = 1; got > 0 && length + 1u < capacity; length += static_cast<std::size_t>(got > 0 ? got : 0))
        got = read(client, answer + length, capacity - 1u - length);
    answer[length] = '\0';
    close(client);
    return length;
}

std::size_t read_file(const char *path, char *text, std::size_t capacity)
{
    const int file = open(path, O_RDONLY);
    if (file < 0) return 0u;
    const ssize_t got = read(file, text, capacity - 1u);
    close(file);
    text[got > 0 ? got : 0] = '\0';
    return got > 0 ? static_cast<std::size_t>(got) : 0u;
}

}  // namespace

TEST(posix_trace_host, answers_this_thread_and_a_rising_clock)
{ av::PosixTraceHost host;
  CHECK_EQ(host.thread_key(), static_cast<std::uint64_t>(gettid()));
  CHECK_EQ(host.thread_number(), static_cast<std::uint32_t>(gettid()));
  CHECK_EQ(host.level(), 0u);
  const std::uint64_t before = host.clock();
  usleep(2000);
  CHECK(host.clock() >= before + 10000u);
  std::uint64_t word = 3u;
  CHECK(!host.exchange(word, 4u, 5u));
  CHECK_EQ(word, 3u);
  CHECK(host.exchange(word, 3u, 5u));
  CHECK_EQ(word, 5u); }

TEST(shm_trace_region, makes_a_zeroed_file_and_sweeps_only_dead_processes)
{ char stale[64];
  std::snprintf(stale, sizeof stale, "/dev/shm/rvk-trace-device-%d", 999999999);
  char alive[64];
  std::snprintf(alive, sizeof alive, "/dev/shm/rvk-state-other-%d.sock", static_cast<int>(getpid()));
  char kept[64];
  std::snprintf(kept, sizeof kept, "/dev/shm/crash-device-%d.txt", 999999999);
  const char *const made[] = {stale, alive, kept};
  for (const char *path : made) close(open(path, O_CREAT | O_WRONLY, 0600));
  ShmTraceRegion region;
  CHECK_STATUS(region.open("unit", 4096u), Status::ok);
  CHECK(region.region() != nullptr);
  CHECK_EQ(region.bytes(), 4096u);
  CHECK_EQ(region.process(), static_cast<std::uint32_t>(getpid()));
  CHECK_EQ(region.region()[4095], 0u);
  CHECK_STATUS(region.open("unit", 4096u), Status::invalid_state);
  CHECK(access(stale, F_OK) != 0);
  CHECK(access(alive, F_OK) == 0);
  CHECK(access(kept, F_OK) == 0);
  char mine[64];
  std::snprintf(mine, sizeof mine, "/dev/shm/rvk-trace-unit-%d", static_cast<int>(getpid()));
  struct stat file = {};
  CHECK(stat(mine, &file) == 0);
  CHECK_EQ(static_cast<std::uint64_t>(file.st_size), 4096u);
  CHECK_EQ(file.st_mode & 0777u, 0600u);
  const char *const left[] = {alive, kept, mine};
  for (const char *path : left) unlink(path); }

TEST(shm_trace_region, an_impossible_size_is_refused_and_nothing_is_left)
{ ShmTraceRegion region;
  CHECK_STATUS(region.open("huge", static_cast<std::size_t>(-1) / 2u), Status::no_memory);
  CHECK(region.region() == nullptr);
  char mine[64];
  std::snprintf(mine, sizeof mine, "/dev/shm/rvk-trace-huge-%d", static_cast<int>(getpid()));
  CHECK(access(mine, F_OK) != 0); }

TEST(posix_module_resolver, names_the_object_an_address_is_in)
{ av::PosixModuleResolver resolver;
  char     name[64] = {};
  std::uint64_t offset = 0u;
  const std::uint64_t in_libc = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&getpid));
  CHECK(resolver.resolve(in_libc, name, sizeof name, offset));
  CHECK(std::strncmp(name, "libc.so", 7u) == 0);
  CHECK(offset != 0u && offset < in_libc);
  const std::uint64_t in_test = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&read_file));
  CHECK(resolver.resolve(in_test, name, sizeof name, offset));
  CHECK(std::strcmp(name, "test_device_trace") == 0);
  char tiny[4];
  CHECK(resolver.resolve(in_libc, tiny, sizeof tiny, offset));
  CHECK(std::strcmp(tiny, "lib") == 0);
  CHECK(!resolver.resolve(in_libc, tiny, 0u, offset));
  CHECK(!resolver.resolve(16u, name, sizeof name, offset)); }

TEST(device_trace, serves_its_table_its_state_and_a_crash_file)
{ struct sigaction counting = {};
  counting.sa_sigaction = &count_bus;
  counting.sa_flags = SA_SIGINFO;
  sigaction(SIGBUS, &counting, nullptr);
  /* An ignored signal: the chain gives it back and raises it again, which an
   * ignored disposition drops. */
  signal(SIGILL, SIG_IGN);
  services.start(root, &schedule, &forward_crash);
  services.start(root, &schedule, &forward_crash);

  /* The table: in /dev/shm, laid out for this process, bound. */
  char table_path[64];
  path_for(table_path, sizeof table_path, "/dev/shm/rvk-trace-device-%d");
  const int table_file = open(table_path, O_RDONLY);
  CHECK(table_file >= 0);
  struct stat file = {};
  fstat(table_file, &file);
  auto *const bytes = static_cast<std::uint8_t *>(mmap(nullptr, static_cast<std::size_t>(file.st_size), PROT_READ, MAP_SHARED, table_file, 0));
  close(table_file);
  TraceSnapshot snapshot;
  CHECK_STATUS(snapshot.read(bytes, static_cast<std::size_t>(file.st_size)), Status::ok);
  CHECK_EQ(snapshot.slots(), HostTrace::slots);
  CHECK(std::strcmp(snapshot.module(), "device") == 0);
  CHECK_EQ(snapshot.process(), static_cast<std::uint32_t>(getpid()));
  CHECK(av::scope_binding.table() != nullptr);

  /* The socket: a dump, a request it does not know, and a main loop that
   * never answers. */
  char answer[512];
  ask("state\n", answer, sizeof answer);
  CHECK(std::strcmp(answer, "module/0 devices=0\n") == 0);
  ask("status\n", answer, sizeof answer);
  CHECK(std::strcmp(answer, "error unknown request\n") == 0);
  ask("st", answer, sizeof answer);
  CHECK(std::strcmp(answer, "error unknown request\n") == 0);
  main_loop_answers = false;
  ask("state\n", answer, sizeof answer);
  CHECK(std::strcmp(answer, "error the main loop did not answer within 2 s\n") == 0);
  /* The routine that never ran is still owed: a request now is refused until
   * it runs and clears the abandoned one. */
  ask("state\n", answer, sizeof answer);
  CHECK(std::strcmp(answer, "error the main loop has not finished the previous request\n") == 0);
  main_loop_answers = true;
  ask("state\n", answer, sizeof answer);
  CHECK(std::strcmp(answer, "error the main loop has not finished the previous request\n") == 0);

  /* A machine check is QEMU's own: chained, nothing written. */
  char crash_path[64];
  path_for(crash_path, sizeof crash_path, "/dev/shm/crash-device-%d.txt");
  unlink(crash_path);
  siginfo_t machine = {};
  machine.si_code = BUS_MCEERR_AR;
  services.crashed(SIGBUS, &machine, nullptr);
  CHECK_EQ(bus_handled, 1);
  CHECK(access(crash_path, F_OK) != 0);

  /* A real crash: recorded, written once with this thread's frames, chained. */
  siginfo_t fault = {};
  fault.si_code = SEGV_MAPERR;
  fault.si_addr = reinterpret_cast<void *>(static_cast<std::uintptr_t>(0x1234u));
  {
    static constexpr av::ScopeName scope("device_trace_test::crash");
    const std::uint32_t slot = av::scope_binding.table()->enter(scope);
    services.crashed(SIGBUS, &fault, nullptr);
    av::scope_binding.table()->leave(slot);
  }
  CHECK_EQ(bus_handled, 2);
  char crash[8192];
  CHECK(read_file(crash_path, crash, sizeof crash) > 0u);
  char first_line[128];
  std::snprintf(first_line, sizeof first_line,
                "crash module=device pid=%d code=0x00000007 address=0x0000000000001234\n", static_cast<int>(getpid()));
  CHECK(std::strncmp(crash, first_line, std::strlen(first_line)) == 0);
  CHECK(std::strstr(crash, "\ntrace module=device pid=") != nullptr);
  CHECK(std::strstr(crash, "  #0 device_trace_test::crash irql=0") != nullptr);
  CHECK(std::strstr(crash, "\nnative #0 0x") != nullptr);
  CHECK(std::strstr(crash, "(test_device_trace+0x") != nullptr);
  /* The walk ends at the last real frame, never at a zero address. */
  CHECK(std::strstr(crash, " 0x0000000000000000\n") == nullptr);
  CHECK_STATUS(snapshot.read(bytes, static_cast<std::size_t>(file.st_size)), Status::ok);
  CHECK_EQ(snapshot.crash_code(), 7u);
  CHECK_EQ(snapshot.crash_address(), 0x1234u);
  /* A second crash chains but writes nothing more. */
  unlink(crash_path);
  services.crashed(SIGBUS, &fault, nullptr);
  CHECK_EQ(bus_handled, 3);
  CHECK(access(crash_path, F_OK) != 0);
  /* A signal whose earlier action was not a handler gets it back and is
   * raised into it: a signal another process sent re-runs nothing. */
  services.crashed(SIGILL, &fault, nullptr);
  struct sigaction now = {};
  sigaction(SIGILL, nullptr, &now);
  CHECK(now.sa_handler == SIG_IGN);

  munmap(bytes, static_cast<std::size_t>(file.st_size));
  av::scope_binding.unbind();
  unlink(table_path); }
