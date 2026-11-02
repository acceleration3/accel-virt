#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"
#include "executor/shim/reporter_state_source.hpp"
#include "device/shim/state_socket.hpp"
#include "device/shim/state_source.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using av::ReporterStateSource;
using av::StateSocket;

namespace {

class TwoSessions final : public av::StateReporter {
public:
    void report(av::StateWriter &writer) override
    {
        writer.open("executor", 0u);
        writer.number("live", 2u);
        writer.close();
    }
};

class Refusing final : public av::StateSource {
public:
    std::size_t dump(char *, std::size_t, const char *&error) override
    {
        error = "error nothing to say\n";
        return 0u;
    }
};

std::size_t ask(const char *module, const char *request, char *answer, std::size_t capacity)
{
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    std::snprintf(address.sun_path, sizeof address.sun_path, "/dev/shm/av-state-%s-%d.sock", module,
                  static_cast<int>(getpid()));
    const int client = socket(AF_UNIX, SOCK_STREAM, 0);
    std::size_t length = 0u;
    if (connect(client, static_cast<sockaddr *>(static_cast<void *>(&address)), sizeof address) == 0) {
        CHECK(write(client, request, std::strlen(request)) == static_cast<ssize_t>(std::strlen(request)));
        for (ssize_t got = 1; got > 0 && length + 1u < capacity;) {
            got = read(client, answer + length, capacity - 1u - length);
            if (got > 0) length += static_cast<std::size_t>(got);
        }
    }
    answer[length] = '\0';
    close(client);
    return length;
}

TwoSessions         root;
ReporterStateSource source(root);
StateSocket         executor;
Refusing            refusing;
StateSocket         silent;

}

TEST(state_socket, answers_a_reporter_s_dump_and_refuses_what_it_does_not_know)
{ const auto pid = static_cast<std::uint32_t>(getpid());
  CHECK(executor.start("executor", pid, source));
  CHECK(!executor.start("executor", pid, source));
  char answer[256];
  ask("executor", "state\n", answer, sizeof answer);
  CHECK(std::strcmp(answer, "executor/0 live=2\n") == 0);
  ask("executor", "status\n", answer, sizeof answer);
  CHECK(std::strcmp(answer, "error unknown request\n") == 0);
  ask("executor", "state", answer, sizeof answer);
  CHECK(std::strcmp(answer, "error unknown request\n") == 0);
  CHECK(silent.start("silent", pid, refusing));
  ask("silent", "state\n", answer, sizeof answer);
  CHECK(std::strcmp(answer, "error nothing to say\n") == 0);
  char path[108];
  std::snprintf(path, sizeof path, "/dev/shm/av-state-executor-%d.sock", static_cast<int>(pid));
  unlink(path);
  std::snprintf(path, sizeof path, "/dev/shm/av-state-silent-%d.sock", static_cast<int>(pid));
  unlink(path); }

TEST(state_socket, a_name_too_long_for_a_socket_path_is_refused)
{ StateSocket named;
  const char *const long_module = "a-module-name-long-enough-that-the-socket-path-cannot-hold-it-whole-in-108-bytes-at-all";
  CHECK(!named.start(long_module, 1u, source));
  DIR *const shm = opendir("/dev/shm");
  CHECK(shm != nullptr);
  bool left = false;
  for (dirent *entry = readdir(shm); entry != nullptr; entry = readdir(shm))
    left = left || std::strncmp(entry->d_name, "av-state-a-module-name", 23u) == 0;
  closedir(shm);
  CHECK(!left); }
