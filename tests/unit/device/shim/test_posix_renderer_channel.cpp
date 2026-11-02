#include "core/support/status.hpp"
#include "device/shim/posix_renderer_channel.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using av::PosixRendererChannel;
using av::Status;

namespace {
class Pair {
public:
    Pair() : ends_{-1, -1} { static_cast<void>(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, ends_)); }
    ~Pair()
    {
        if (ends_[1] >= 0) ::close(ends_[1]);
    }
    int  channel_end() const { return ends_[0]; }
    int  qemu_end() const { return ends_[1]; }
    void close_qemu_end()
    {
        ::close(ends_[1]);
        ends_[1] = -1;
    }
    bool send(const std::uint8_t *bytes, std::size_t length, const int *descriptors, std::uint32_t count)
    {
        alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * 128u)] = {};
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
        return ::sendmsg(ends_[1], &message, MSG_NOSIGNAL) == static_cast<ssize_t>(length);
    }

private:
    int ends_[2];
};

class Pipe {
public:
    Pipe() : ends_{-1, -1} { static_cast<void>(::pipe2(ends_, O_CLOEXEC | O_NONBLOCK)); }
    ~Pipe()
    {
        ::close(ends_[0]);
        if (ends_[1] >= 0) ::close(ends_[1]);
    }
    int  write_end() const { return ends_[1]; }
    void close_write_end()
    {
        ::close(ends_[1]);
        ends_[1] = -1;
    }
    bool all_writers_closed() const
    {
        char byte = 0;
        return ::read(ends_[0], &byte, 1u) == 0;
    }

private:
    int ends_[2];
};

bool same_file(int first, int second)
{
    struct stat a = {}, b = {};
    return ::fstat(first, &a) == 0 && ::fstat(second, &b) == 0 && a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

}

TEST(posix_renderer_channel_receive, takes_a_datagram_and_its_descriptors)
{ Pair pair;
  PosixRendererChannel channel(pair.channel_end());
  CHECK_EQ(channel.descriptor(), pair.channel_end());
  Pipe first, second;
  const int          sent[2] = {first.write_end(), second.write_end()};
  const std::uint8_t bytes[5] = {1u, 2u, 3u, 4u, 5u};
  CHECK(pair.send(bytes, sizeof bytes, sent, 2u));
  std::uint8_t  buffer[64] = {};
  std::size_t   length = 0u;
  int           descriptors[PosixRendererChannel::max_descriptors] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(channel.receive(buffer, sizeof buffer, length, descriptors, count), Status::ok);
  CHECK_EQ(length, sizeof bytes);
  CHECK(std::memcmp(buffer, bytes, sizeof bytes) == 0);
  CHECK_EQ(count, 2u);
  CHECK(same_file(descriptors[0], first.write_end()));
  CHECK(same_file(descriptors[1], second.write_end()));
  CHECK(descriptors[0] != first.write_end());
  CHECK((::fcntl(descriptors[0], F_GETFD) & FD_CLOEXEC) != 0);
  ::close(descriptors[0]);
  ::close(descriptors[1]);
  CHECK(pair.send(bytes, 3u, nullptr, 0u));
  CHECK_STATUS(channel.receive(buffer, sizeof buffer, length, descriptors, count), Status::ok);
  CHECK_EQ(length, 3u);
  CHECK_EQ(count, 0u); }

TEST(posix_renderer_channel_receive, a_datagram_too_long_is_too_large_and_its_descriptors_closed)
{ Pair pair;
  PosixRendererChannel channel(pair.channel_end());
  Pipe pipe;
  const int          sent = pipe.write_end();
  const std::uint8_t bytes[16] = {7u};
  CHECK(pair.send(bytes, sizeof bytes, &sent, 1u));
  std::uint8_t  buffer[15] = {};
  std::size_t   length = 9u;
  int           descriptors[PosixRendererChannel::max_descriptors] = {};
  std::uint32_t count = 9u;
  CHECK_STATUS(channel.receive(buffer, sizeof buffer, length, descriptors, count), Status::too_large);
  CHECK_EQ(length, 0u);
  CHECK_EQ(count, 0u);
  pipe.close_write_end();
  CHECK(pipe.all_writers_closed());
  CHECK(pair.send(bytes, sizeof buffer, nullptr, 0u));
  CHECK_STATUS(channel.receive(buffer, sizeof buffer, length, descriptors, count), Status::ok);
  CHECK_EQ(length, sizeof buffer); }

TEST(posix_renderer_channel_receive, more_descriptors_than_any_message_carries_are_all_closed)
{ Pair pair;
  PosixRendererChannel channel(pair.channel_end());
  Pipe pipe;
  constexpr std::uint32_t too_many = PosixRendererChannel::max_descriptors + 1u;
  int sent[too_many];
  for (int &descriptor : sent) descriptor = pipe.write_end();
  const std::uint8_t bytes[4] = {1u};
  CHECK(pair.send(bytes, sizeof bytes, sent, too_many));
  std::uint8_t  buffer[16] = {};
  std::size_t   length = 0u;
  int           descriptors[PosixRendererChannel::max_descriptors] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(channel.receive(buffer, sizeof buffer, length, descriptors, count), Status::too_large);
  CHECK_EQ(count, 0u);
  pipe.close_write_end();
  CHECK(pipe.all_writers_closed()); }

TEST(posix_renderer_channel_receive, as_many_descriptors_as_a_message_carries_arrive)
{ Pair pair;
  PosixRendererChannel channel(pair.channel_end());
  Pipe pipe;
  int sent[PosixRendererChannel::max_descriptors];
  for (int &descriptor : sent) descriptor = pipe.write_end();
  const std::uint8_t bytes[4] = {1u};
  CHECK(pair.send(bytes, sizeof bytes, sent, PosixRendererChannel::max_descriptors));
  std::uint8_t  buffer[16] = {};
  std::size_t   length = 0u;
  int           descriptors[PosixRendererChannel::max_descriptors] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(channel.receive(buffer, sizeof buffer, length, descriptors, count), Status::ok);
  CHECK_EQ(count, PosixRendererChannel::max_descriptors);
  for (std::uint32_t index = 0u; index < count; index++) ::close(descriptors[index]);
  pipe.close_write_end();
  CHECK(pipe.all_writers_closed()); }

TEST(posix_renderer_channel, a_qemu_that_went_away_is_not_found_both_ways)
{ Pair pair;
  PosixRendererChannel channel(pair.channel_end());
  pair.close_qemu_end();
  std::uint8_t  buffer[16] = {};
  std::size_t   length = 5u;
  int           descriptors[PosixRendererChannel::max_descriptors] = {};
  std::uint32_t count = 5u;
  CHECK_STATUS(channel.receive(buffer, sizeof buffer, length, descriptors, count), Status::not_found);
  CHECK_EQ(length, 0u);
  CHECK_EQ(count, 0u);
  CHECK_STATUS(channel.send(buffer, 4u, nullptr, 0u), Status::not_found); }

TEST(posix_renderer_channel_send, sends_the_answer_and_its_descriptor)
{ Pair pair;
  PosixRendererChannel channel(pair.channel_end());
  Pipe pipe;
  const int          image = pipe.write_end();
  const std::uint8_t bytes[6] = {9u, 8u, 7u, 6u, 5u, 4u};
  CHECK_STATUS(channel.send(bytes, sizeof bytes, &image, 1u), Status::ok);
  CHECK_STATUS(channel.send(bytes, 2u, nullptr, 0u), Status::ok);
  alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * 4u)] = {};
  std::uint8_t          buffer[16] = {};
  iovec                 vector = {buffer, sizeof buffer};
  msghdr                message = {};
  message.msg_iov = &vector;
  message.msg_iovlen = 1u;
  message.msg_control = control;
  message.msg_controllen = sizeof control;
  CHECK_EQ(::recvmsg(pair.qemu_end(), &message, 0), static_cast<ssize_t>(sizeof bytes));
  CHECK(std::memcmp(buffer, bytes, sizeof bytes) == 0);
  cmsghdr *const header = CMSG_FIRSTHDR(&message);
  CHECK(header != nullptr && header->cmsg_type == SCM_RIGHTS);
  int received = -1;
  if (header != nullptr) std::memcpy(&received, CMSG_DATA(header), sizeof received);
  CHECK(same_file(received, image));
  ::close(received);
  CHECK(::fcntl(image, F_GETFD) != -1);
  message.msg_controllen = sizeof control;
  CHECK_EQ(::recvmsg(pair.qemu_end(), &message, 0), 2);
  CHECK(CMSG_FIRSTHDR(&message) == nullptr); }

TEST(posix_renderer_channel_send, refuses_more_descriptors_than_a_message_carries_or_none_to_carry)
{ Pair pair;
  PosixRendererChannel channel(pair.channel_end());
  const std::uint8_t bytes[1] = {1u};
  int many[PosixRendererChannel::max_descriptors + 1u] = {};
  CHECK_STATUS(channel.send(bytes, 1u, many, PosixRendererChannel::max_descriptors + 1u), Status::invalid_parameter);
  CHECK_STATUS(channel.send(bytes, 1u, nullptr, 1u), Status::invalid_parameter); }

TEST(posix_renderer_channel, closes_its_socket_when_it_goes)
{ Pair pair;
  { PosixRendererChannel channel(pair.channel_end()); }
  std::uint8_t byte = 0u;
  CHECK_EQ(::recv(pair.qemu_end(), &byte, 1u, 0), 0); }
