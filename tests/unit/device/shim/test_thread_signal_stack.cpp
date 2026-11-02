#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/scope_name.hpp"
#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"
#include "device/shim/device_trace.hpp"
#include "device/shim/posix_trace_host.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" const char *__asan_default_options();
extern "C" const char *__asan_default_options() { return "handle_segv=0:use_sigaltstack=0:detect_stack_use_after_return=0"; }

namespace {

constexpr std::size_t least_stack = 64u * 1024u;

struct Seen {
    stack_t before;
    stack_t after_first;
    stack_t after_second;
    bool    guard_below;
};

bool guarded_below(const void *address)
{
    const auto below = static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(address)) - 1u;
    FILE *const maps = std::fopen("/proc/self/maps", "r");
    char        line[512];
    bool        guarded = false;
    while (maps != nullptr && std::fgets(line, sizeof line, maps) != nullptr) {
        unsigned long start = 0u;
        unsigned long end = 0u;
        char          permissions[5] = {};
        if (std::sscanf(line, "%lx-%lx %4s", &start, &end, permissions) != 3) continue;
        if (below >= start && below < end) guarded = std::strcmp(permissions, "---p") == 0;
    }
    if (maps != nullptr) std::fclose(maps);
    return guarded;
}

bool mapped(std::uintptr_t at)
{
    FILE *const maps = std::fopen("/proc/self/maps", "r");
    char        line[512];
    bool        found = false;
    while (maps != nullptr && std::fgets(line, sizeof line, maps) != nullptr) {
        unsigned long start = 0u;
        unsigned long end = 0u;
        if (std::sscanf(line, "%lx-%lx", &start, &end) == 2 && at >= start && at < end) found = true;
    }
    if (maps != nullptr) std::fclose(maps);
    return found;
}

void *ask_twice(void *context)
{
    auto *const seen = static_cast<Seen *>(context);
    const av::PosixTraceHost host;
    sigaltstack(nullptr, &seen->before);
    static_cast<void>(host.thread_number());
    sigaltstack(nullptr, &seen->after_first);
    static_cast<void>(host.thread_key());
    sigaltstack(nullptr, &seen->after_second);
    seen->guard_below = guarded_below(seen->after_first.ss_sp);
    return nullptr;
}

char own_stack[128u * 1024u];

void *ask_with_own_stack(void *context)
{
    auto *const seen = static_cast<Seen *>(context);
    stack_t mine = {};
    mine.ss_sp = own_stack;
    mine.ss_size = sizeof own_stack;
    sigaltstack(&mine, nullptr);
    const av::PosixTraceHost host;
    static_cast<void>(host.thread_number());
    sigaltstack(nullptr, &seen->after_first);
    return nullptr;
}

class NoDevices final : public av::StateReporter {
public:
    void report(av::StateWriter &writer) override
    {
        writer.open("module", 0u);
        writer.close();
    }
};

NoDevices           root;
av::DeviceTraceRoot services;

void forward_crash(int signal, siginfo_t *info, void *context) { services.crashed(signal, info, context); }
void never_scheduled(AvHostRoutine, void *) {}

[[gnu::noinline]] std::uint64_t recurse(std::uint64_t depth)
{
    static constexpr av::ScopeName name("overflow_test::recurse");
    const av::FunctionScope scope(av::scope_binding, name);
    volatile char ballast[4096];
    ballast[0] = static_cast<char>(depth);
    if (depth > (1u << 30u)) return depth;
    return recurse(depth + 1u) + static_cast<std::uint64_t>(ballast[0]);
}

void *overflow(void *)
{
    static_cast<void>(recurse(0u));
    return nullptr;
}

}

TEST(posix_trace_host, a_thread_gets_a_guarded_alternate_signal_stack_once)
{ Seen seen = {};
  pthread_t thread = {};
  CHECK_EQ(pthread_create(&thread, nullptr, &ask_twice, &seen), 0);
  CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK((seen.before.ss_flags & SS_DISABLE) != 0);
  CHECK((seen.after_first.ss_flags & SS_DISABLE) == 0);
  CHECK(seen.after_first.ss_size >= least_stack);
  CHECK(seen.after_first.ss_size >= 4u * static_cast<std::size_t>(sysconf(_SC_SIGSTKSZ)));
  CHECK(seen.guard_below);
  CHECK(seen.after_second.ss_sp == seen.after_first.ss_sp);
  CHECK_EQ(seen.after_second.ss_size, seen.after_first.ss_size);
  const auto stack = static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(seen.after_first.ss_sp));
  CHECK(stack != 0u);
  CHECK(!mapped(stack));
  CHECK(!mapped(stack - 1u)); }

TEST(posix_trace_host, a_thread_s_own_alternate_stack_is_kept)
{ Seen seen = {};
  pthread_t thread = {};
  CHECK_EQ(pthread_create(&thread, nullptr, &ask_with_own_stack, &seen), 0);
  CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK(seen.after_first.ss_sp == own_stack);
  CHECK_EQ(seen.after_first.ss_size, sizeof own_stack); }

TEST(device_trace, a_stack_overflow_on_any_thread_writes_the_crash_file)
{ const pid_t child = fork();
  CHECK(child >= 0);
  if (child == 0) {
    setenv("AV_TRACE_SCOPES", "1", 1);
    services.start(root, &never_scheduled, &forward_crash);
    pthread_attr_t attributes = {};
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 256u * 1024u);
    pthread_t thread = {};
    if (pthread_create(&thread, &attributes, &overflow, nullptr) == 0) pthread_join(thread, nullptr);
    _exit(0);
  }
  int status = 0;
  CHECK_EQ(waitpid(child, &status, 0), child);
  CHECK(WIFSIGNALED(status));
  CHECK_EQ(WTERMSIG(status), SIGSEGV);
  char path[64];
  std::snprintf(path, sizeof path, "/dev/shm/crash-device-%d.txt", static_cast<int>(child));
  char text[16384] = {};
  const int file = open(path, O_RDONLY);
  CHECK(file >= 0);
  const ssize_t got = read(file, text, sizeof text - 1u);
  close(file);
  CHECK(got > 0);
  char first[96];
  std::snprintf(first, sizeof first, "crash module=device pid=%d code=0x0000000B address=0x", static_cast<int>(child));
  CHECK(std::strncmp(text, first, std::strlen(first)) == 0);
  CHECK(std::strstr(text, "\n  #0 overflow_test::recurse irql=0") != nullptr);
  CHECK(std::strstr(text, "\n  #31 overflow_test::recurse irql=0") != nullptr);
  CHECK(std::strstr(text, "deeper frames not stored") != nullptr);
  CHECK(std::strstr(text, "\nnative #0 0x") != nullptr);
  unlink(path);
  char table[64];
  std::snprintf(table, sizeof table, "/dev/shm/av-trace-device-%d", static_cast<int>(child));
  unlink(table);
  char socket_path[64];
  std::snprintf(socket_path, sizeof socket_path, "/dev/shm/av-state-device-%d.sock", static_cast<int>(child));
  unlink(socket_path); }
