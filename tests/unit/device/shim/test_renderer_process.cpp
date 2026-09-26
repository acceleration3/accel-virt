/* rvk-renderer itself, spawned as QEMU spawns it: its socket on descriptor
 * 3, greeted with the library to load, answering with Ready and with each
 * request's reply, living through what it refuses, and leaving with status 0
 * when QEMU closes its end. Every wait is bounded. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "harness/framework/test_registry.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/renderer/context_message.hpp"
#include "proto/renderer/export_image_message.hpp"
#include "proto/renderer/exported_image_message.hpp"
#include "proto/renderer/hello_message.hpp"
#include "proto/renderer/ready_message.hpp"
#include "proto/renderer/region_message.hpp"
#include "proto/renderer/renderer_message_header.hpp"
#include "proto/renderer/reply_message.hpp"

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

using av::RendererMessageHeader;
using av::Status;
using Kind = av::RendererMessageHeader::Kind;

namespace {

constexpr int           patience_ms = 20000;
constexpr std::uint8_t  uuid[av::HelloMessage::uuid_bytes] = {0x42u, 7u};
constexpr std::size_t   capacity = RendererMessageHeader::encoded_size + 8192u;

/* One rvk-renderer, its socket's QEMU end, and its stderr. */
class Renderer {
public:
    Renderer() : pid_(-1), socket_(-1), log_(-1), log_text_{}
    {
        int pair[2] = {-1, -1};
        int logs[2] = {-1, -1};
        if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair) != 0 || ::pipe2(logs, O_CLOEXEC) != 0) return;
        /* Above 3, so the dup2 to 3 is a real one and clears close-on-exec. */
        const int child = ::fcntl(pair[1], F_DUPFD_CLOEXEC, 10);
        ::close(pair[1]);
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, child, 3);
        posix_spawn_file_actions_adddup2(&actions, logs[1], 2);
        char        path[] = AV_RENDERER_BINARY;
        char *const argv[] = {path, nullptr};
        if (::posix_spawn(&pid_, path, &actions, nullptr, argv, environ) != 0) pid_ = -1;
        posix_spawn_file_actions_destroy(&actions);
        ::close(child);
        ::close(logs[1]);
        socket_ = pair[0];
        log_ = logs[0];
    }
    ~Renderer()
    {
        if (socket_ >= 0) ::close(socket_);
        if (pid_ > 0) {
            ::kill(pid_, SIGKILL);
            int status = 0;
            ::waitpid(pid_, &status, 0);
        }
        if (log_ >= 0) ::close(log_);
    }

    bool started() const { return pid_ > 0 && socket_ >= 0; }

    bool send(Kind kind, std::uint64_t sequence, const std::uint8_t *body, std::uint32_t length,
              const int *descriptors = nullptr, std::uint32_t count = 0u)
    {
        std::uint8_t datagram[capacity] = {};
        if (RendererMessageHeader(kind, length, sequence).encode(datagram, sizeof datagram) != Status::ok) return false;
        if (length != 0u) std::memcpy(datagram + RendererMessageHeader::encoded_size, body, length);
        return raw(datagram, RendererMessageHeader::encoded_size + length, descriptors, count);
    }
    bool raw(const std::uint8_t *bytes, std::size_t length, const int *descriptors, std::uint32_t count)
    {
        alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * 4u)] = {};
        iovec  vector = {const_cast<std::uint8_t *>(bytes), length};
        msghdr message = {};
        message.msg_iov = &vector;
        message.msg_iovlen = 1u;
        if (count != 0u) {
            message.msg_control = control;
            message.msg_controllen = CMSG_SPACE(sizeof(int) * count);
            cmsghdr *const header = CMSG_FIRSTHDR(&message);
            header->cmsg_level = SOL_SOCKET;
            header->cmsg_type = SCM_RIGHTS;
            header->cmsg_len = CMSG_LEN(sizeof(int) * count);
            std::memcpy(CMSG_DATA(header), descriptors, sizeof(int) * count);
        }
        return ::sendmsg(socket_, &message, MSG_NOSIGNAL) == static_cast<ssize_t>(length);
    }
    /* The next datagram within `milliseconds`: its header, body and length;
     * false when none came. */
    bool receive(RendererMessageHeader &header, std::uint8_t *body, std::uint32_t &length, int milliseconds)
    {
        pollfd watched = {socket_, POLLIN, 0};
        if (::poll(&watched, 1u, milliseconds) != 1) return false;
        std::uint8_t  datagram[capacity] = {};
        const ssize_t got = ::recv(socket_, datagram, sizeof datagram, 0);
        if (got <= 0 || header.decode(datagram, static_cast<std::size_t>(got)) != Status::ok) return false;
        length = header.body_length();
        std::memcpy(body, datagram + RendererMessageHeader::encoded_size, length);
        return true;
    }
    Status reply(std::uint64_t sequence)
    {
        RendererMessageHeader header;
        std::uint8_t          body[capacity] = {};
        std::uint32_t         length = 0u;
        av::ReplyMessage      reply;
        if (!receive(header, body, length, patience_ms) || header.kind() != Kind::reply ||
            header.sequence() != sequence || reply.decode(body, length, 0u) != Status::ok)
            return Status::malformed;
        return reply.status();
    }
    /* QEMU goes: the socket closes, and the renderer's exit status within
     * the patience, or -1. */
    int leave()
    {
        ::close(socket_);
        socket_ = -1;
        for (int waited = 0; waited < patience_ms; waited += 10) {
            int         status = 0;
            const pid_t done = ::waitpid(pid_, &status, WNOHANG);
            if (done == pid_) {
                pid_ = -1;
                return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            }
            ::usleep(10000u);
        }
        return -1;
    }
    /* What it wrote to stderr, once it has left. */
    const char *log()
    {
        std::size_t used = 0u;
        for (;;) {
            const ssize_t got = ::read(log_, log_text_ + used, sizeof log_text_ - 1u - used);
            if (got <= 0) break;
            used += static_cast<std::size_t>(got);
        }
        log_text_[used] = '\0';
        return log_text_;
    }

private:
    pid_t pid_;
    int   socket_;
    int   log_;
    char  log_text_[8192];
};

std::uint32_t hello_body(std::uint8_t *body, std::size_t room, const char *library)
{
    const av::CapsetInfo capset(av::ProtocolVersion(av::ProtocolVersion::current_major,
                                                    av::ProtocolVersion::current_minor),
                                RVK_EXECUTOR_SUB_API_VULKAN, 64u, 12u, 24u, 0u,
                                av::GpuIdentity(0x1002u, 0x7550u, av::GpuIdentity::discrete_gpu, uuid, 1u, 1u, "gpu"));
    std::uint8_t capset_bytes[av::CapsetInfo::encoded_size] = {};
    static_cast<void>(capset.encode(capset_bytes, sizeof capset_bytes));
    const av::HelloMessage hello(1u, 1u, uuid, library, static_cast<std::uint32_t>(std::strlen(library)), capset_bytes,
                                 sizeof capset_bytes);
    static_cast<void>(hello.encode(body, room));
    return static_cast<std::uint32_t>(hello.encoded_length());
}

}  // namespace

/* Greeted with the echo executor, the renderer loads it and answers Ready
 * with what it offers; it serves each request, lives through what it
 * refuses, and leaves cleanly when QEMU does. */
TEST(renderer_process, greets_serves_survives_refusals_and_leaves_when_qemu_does)
{ Renderer renderer;
  CHECK(renderer.started());
  std::uint8_t body[capacity] = {};
  CHECK(renderer.send(Kind::hello, 1u, body, hello_body(body, sizeof body, AV_ECHO_EXECUTOR)));
  RendererMessageHeader header;
  std::uint8_t          answer[capacity] = {};
  std::uint32_t         length = 0u;
  CHECK(renderer.receive(header, answer, length, patience_ms));
  CHECK(header.kind() == Kind::ready);
  CHECK_EQ(header.sequence(), 1u);
  av::ReadyMessage ready;
  CHECK_STATUS(ready.decode(answer, length, 0u), Status::ok);
  CHECK_EQ(ready.version(), 1u);
  CHECK((ready.sub_apis() & RVK_EXECUTOR_SUB_API_VULKAN) != 0u);

  /* Guest RAM crosses as a memfd and is answered nothing. */
  const int ram = ::memfd_create("guest-ram", MFD_CLOEXEC);
  CHECK_EQ(::ftruncate(ram, 0x10000), 0);
  std::uint8_t region[av::RegionMessage::encoded_size] = {};
  CHECK_STATUS(av::RegionMessage(0u, 0x10000u).encode(region, sizeof region), Status::ok);
  CHECK(renderer.send(Kind::region, 2u, region, sizeof region, &ram, 1u));
  ::close(ram);

  std::uint8_t context[av::ContextMessage::encoded_size] = {};
  CHECK_STATUS(av::ContextMessage(4u).encode(context, sizeof context), Status::ok);
  CHECK(renderer.send(Kind::create_context, 3u, context, sizeof context));
  CHECK_STATUS(renderer.reply(3u), Status::ok);

  /* Garbage is refused and answered nothing; the renderer serves on. */
  const std::uint8_t garbage[7] = {0xFFu, 1u, 2u, 3u, 4u, 5u, 6u};
  CHECK(renderer.raw(garbage, sizeof garbage, nullptr, 0u));
  CHECK(renderer.send(Kind::create_context, 4u, context, sizeof context));
  CHECK_STATUS(renderer.reply(4u), Status::conflict);

  /* An image nothing published is answered as nothing found. */
  std::uint8_t key[av::ExportImageMessage::encoded_size] = {};
  CHECK_STATUS(av::ExportImageMessage(9u).encode(key, sizeof key), Status::ok);
  CHECK(renderer.send(Kind::export_image, 5u, key, sizeof key));
  CHECK(renderer.receive(header, answer, length, patience_ms));
  CHECK(header.kind() == Kind::exported_image);
  CHECK_EQ(header.sequence(), 5u);
  av::ExportedImageMessage exported;
  CHECK_STATUS(exported.decode(answer, length, 0u), Status::ok);
  CHECK(!exported.found());

  CHECK(renderer.send(Kind::destroy_context, 6u, context, sizeof context));
  CHECK_STATUS(renderer.reply(6u), Status::ok);
  CHECK_EQ(renderer.leave(), 0);
  CHECK(std::strstr(renderer.log(), "rvk-renderer: a message from QEMU was refused") != nullptr); }

/* A library that does not load is the hello's refusal: nothing is answered,
 * and a hello naming one that does is answered after it. */
TEST(renderer_process, a_library_that_does_not_load_is_answered_nothing)
{ Renderer renderer;
  CHECK(renderer.started());
  std::uint8_t body[capacity] = {};
  CHECK(renderer.send(Kind::hello, 1u, body, hello_body(body, sizeof body, "/no/such/librvk_executor.so")));
  RendererMessageHeader header;
  std::uint8_t          answer[capacity] = {};
  std::uint32_t         length = 0u;
  CHECK(!renderer.receive(header, answer, length, 500));
  CHECK(renderer.send(Kind::hello, 2u, body, hello_body(body, sizeof body, AV_ECHO_EXECUTOR)));
  CHECK(renderer.receive(header, answer, length, patience_ms));
  CHECK(header.kind() == Kind::ready);
  CHECK_EQ(header.sequence(), 2u);
  CHECK_EQ(renderer.leave(), 0); }

/* A renderer QEMU never speaks to leaves as soon as QEMU does. */
TEST(renderer_process, leaves_at_once_when_qemu_closes_first)
{ Renderer renderer;
  CHECK(renderer.started());
  CHECK_EQ(renderer.leave(), 0); }
