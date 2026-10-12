#include "data/StateClient.h"

#include "data/Clock.h"
#include "text/Format.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace inspector {
namespace {

// A dump far past any binary's: one that keeps coming is not a dump.
constexpr usize kMaxDumpBytes = 64u << 20;

int RemainingMs(u64 deadline) {
    const u64 now = NowTicks();
    return now >= deadline ? 0 : static_cast<int>((deadline - now) / (kTicksPerSecond / 1000));
}

} // namespace

Optional<String> FetchState(StringView socket_path, int timeout_ms, String& why) {
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (socket_path.GetSize() >= sizeof address.sun_path) {
        why = "socket path too long";
        return NullOpt;
    }
    memcpy(address.sun_path, socket_path.GetData(), socket_path.GetSize());
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        why = Format("socket: {}", strerror(errno));
        return NullOpt;
    }
    const u64 deadline = NowTicks() + static_cast<u64>(timeout_ms) * (kTicksPerSecond / 1000);
    if (connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0) {
        why = Format("connect: {}", strerror(errno));
        close(fd);
        return NullOpt;
    }
    constexpr char kAsk[] = "state\n";
    if (send(fd, kAsk, sizeof kAsk - 1, MSG_NOSIGNAL) != static_cast<ssize_t>(sizeof kAsk - 1)) {
        why = Format("send: {}", strerror(errno));
        close(fd);
        return NullOpt;
    }
    String text;
    char chunk[65536];
    for (;;) {
        pollfd ready{fd, POLLIN, 0};
        const int waited = poll(&ready, 1, RemainingMs(deadline));
        if (waited <= 0) {
            why = waited == 0 ? String("no answer before the deadline") : Format("poll: {}", strerror(errno));
            close(fd);
            return NullOpt;
        }
        const ssize_t got = recv(fd, chunk, sizeof chunk, 0);
        if (got < 0) {
            why = Format("recv: {}", strerror(errno));
            close(fd);
            return NullOpt;
        }
        if (got == 0) {
            break;
        }
        text.Append(chunk, static_cast<usize>(got));
        if (text.GetSize() > kMaxDumpBytes) {
            why = "dump larger than any binary writes";
            close(fd);
            return NullOpt;
        }
    }
    close(fd);
    return text;
}

} // namespace inspector
