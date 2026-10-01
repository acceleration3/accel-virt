#include "device/shim/state_socket.hpp"

#include "core/platform/lock_marks.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "device/shim/state_source.hpp"

#include <cerrno>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

namespace av {

namespace {

constexpr char state_request[] = "state\n";
constexpr char unknown_request[] = "error unknown request\n";
constexpr int  backlog = 4;

void *serve_thread(void *socket)
{
    AV_SCOPE("serve_thread");
    static_cast<StateSocket *>(socket)->serve();
    return nullptr;
}

}  // namespace

StateSocket::StateSocket() : source_(nullptr), listener_(-1), path_{}, thread_(), dump_{}
{
    AV_SCOPE("StateSocket::StateSocket");
}

bool StateSocket::start(const char *module, uint32_t process, StateSource &source)
{
    AV_SCOPE("StateSocket::start");
    if (listener_ >= 0 || !listen_on_socket(module, process)) return false;
    source_ = &source;
    if (pthread_create(&thread_, nullptr, &serve_thread, this) == 0) {
        static_cast<void>(pthread_detach(thread_));
        return true;
    }
    static_cast<void>(close(listener_));
    static_cast<void>(unlink(path_));
    listener_ = -1;
    return false;
}

void StateSocket::serve()
{
    AV_SCOPE("StateSocket::serve");
    const LockMarks doorbell(&listener_);
    for (;;) {
        doorbell.waiting();
        const int client = accept4(listener_, nullptr, nullptr, SOCK_CLOEXEC);
        doorbell.woken();
        if (client >= 0) {
            answer(client);
            static_cast<void>(close(client));
        } else if (errno != EINTR && errno != ECONNABORTED) {
            return;
        }
    }
}

bool StateSocket::listen_on_socket(const char *module, uint32_t process)
{
    AV_SCOPE("StateSocket::listen_on_socket");
    TextBuffer path(path_, sizeof path_);
    path.append("/dev/shm/av-state-");
    path.append(module);
    path.append("-");
    path.append_decimal(process);
    path.append(".sock");
    /* A path cut short would name some other socket. */
    if (path.length() + 1u >= sizeof path_) return false;
    static_cast<void>(unlink(path_));
    listener_ = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listener_ < 0) return false;
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    for (size_t index = 0u; path_[index] != '\0'; index++) address.sun_path[index] = path_[index];
    if (bind(listener_, reinterpret_cast<sockaddr *>(&address), sizeof address) == 0 && chmod(path_, 0600) == 0 &&
        listen(listener_, backlog) == 0)
        return true;
    static_cast<void>(close(listener_));
    static_cast<void>(unlink(path_));
    listener_ = -1;
    return false;
}

void StateSocket::answer(int client)
{
    AV_SCOPE("StateSocket::answer");
    const timeval patience{static_cast<time_t>(answer_seconds), 0};
    static_cast<void>(setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &patience, sizeof patience));
    char   request[request_bytes] = {};
    size_t received = 0u;
    while (received < sizeof request - 1u) {
        const ssize_t got = recv(client, request + received, 1u, 0);
        if (got <= 0) break;
        if (request[received++] == '\n') break;
    }
    if (received != sizeof state_request - 1u || __builtin_memcmp(request, state_request, received) != 0) {
        send(client, unknown_request, sizeof unknown_request - 1u);
        return;
    }
    const char  *error = nullptr;
    const size_t length = source_->dump(dump_, sizeof dump_, error);
    if (error != nullptr) {
        send(client, error, __builtin_strlen(error));
        return;
    }
    send(client, dump_, length);
    send(client, "\n", 1u);
}

void StateSocket::send(int client, const char *text, size_t length) const
{
    AV_SCOPE("StateSocket::send");
    size_t sent = 0u;
    while (sent < length) {
        const ssize_t wrote = ::send(client, text + sent, length - sent, MSG_NOSIGNAL);
        if (wrote <= 0) return;
        sent += static_cast<size_t>(wrote);
    }
}

}  // namespace av
