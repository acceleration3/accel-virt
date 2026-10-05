#include "harness/export/fake_export_consumer.hpp"

#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace av {

FakeExportConsumer::FakeExportConsumer() : consumer_(-1), device_(-1), held_(-1), descriptor_(-1)
{
    int ends[2] = {-1, -1};
    if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, ends) != 0) return;
    consumer_ = ends[0];
    device_ = ends[1];
}

FakeExportConsumer::~FakeExportConsumer()
{
    forget_descriptor();
    leave();
    if (device_ >= 0) ::close(device_);
}

int FakeExportConsumer::device_end()
{
    const int end = device_;
    device_ = -1;
    held_ = end;
    return end;
}

void FakeExportConsumer::refused()
{
    device_ = held_;
    held_ = -1;
}

bool FakeExportConsumer::send(const ExportRequest &request)
{
    std::uint8_t bytes[ExportRequest::encoded_size] = {};
    return request.encode(bytes, sizeof bytes) == Status::ok && send_bytes(bytes, sizeof bytes);
}

bool FakeExportConsumer::send_bytes(const std::uint8_t *bytes, std::size_t length)
{
    return ::send(consumer_, bytes, length, MSG_NOSIGNAL) == static_cast<ssize_t>(length);
}

Status FakeExportConsumer::receive(ExportAnswer &answer, int milliseconds)
{
    forget_descriptor();
    pollfd watched = {consumer_, POLLIN, 0};
    if (::poll(&watched, 1u, milliseconds) != 1) return Status::invalid_state;
    std::uint8_t          bytes[ExportAnswer::encoded_size + 1u] = {};
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * 2u)] = {};
    iovec                 vector = {bytes, sizeof bytes};
    msghdr                message = {};
    message.msg_iov = &vector;
    message.msg_iovlen = 1u;
    message.msg_control = control;
    message.msg_controllen = sizeof control;
    const ssize_t got = ::recvmsg(consumer_, &message, MSG_CMSG_CLOEXEC | MSG_DONTWAIT);
    if (got <= 0) return Status::not_found;
    std::uint32_t count = 0u;
    for (cmsghdr *header = CMSG_FIRSTHDR(&message); header != nullptr; header = CMSG_NXTHDR(&message, header)) {
        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS) continue;
        const std::size_t carried = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (std::size_t index = 0u; index < carried; index++) {
            int descriptor = -1;
            std::memcpy(&descriptor, CMSG_DATA(header) + index * sizeof(int), sizeof descriptor);
            if (count++ == 0u) descriptor_ = descriptor;
            else ::close(descriptor);
        }
    }
    return answer.decode(bytes, static_cast<std::size_t>(got), count);
}

int FakeExportConsumer::descriptor() const
{
    return descriptor_;
}

bool FakeExportConsumer::hung_up(int milliseconds)
{
    pollfd watched = {consumer_, POLLIN, 0};
    if (::poll(&watched, 1u, milliseconds) != 1) return false;
    std::uint8_t byte = 0u;
    return ::recv(consumer_, &byte, sizeof byte, MSG_DONTWAIT) == 0;
}

void FakeExportConsumer::leave()
{
    if (consumer_ >= 0) ::close(consumer_);
    consumer_ = -1;
}

void FakeExportConsumer::forget_descriptor()
{
    if (descriptor_ >= 0) ::close(descriptor_);
    descriptor_ = -1;
}

}  // namespace av
