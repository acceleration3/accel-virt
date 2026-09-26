#include "device/shim/posix_renderer_channel.hpp"

#include "core/trace/function_scope.hpp"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

namespace av {

PosixRendererChannel::PosixRendererChannel(int socket) : socket_(socket)
{
    AV_SCOPE("PosixRendererChannel::PosixRendererChannel");
}

PosixRendererChannel::~PosixRendererChannel()
{
    AV_SCOPE("PosixRendererChannel::~PosixRendererChannel");
    if (socket_ >= 0) close(socket_);
}

int PosixRendererChannel::descriptor() const
{
    AV_SCOPE("PosixRendererChannel::descriptor");
    return socket_;
}

Status PosixRendererChannel::receive(uint8_t *buffer, size_t capacity, size_t &length, int *descriptors,
                                     uint32_t &count)
{
    AV_SCOPE("PosixRendererChannel::receive");
    length = 0u;
    count = 0u;
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * max_descriptors)] = {};
    iovec  vector = {buffer, capacity};
    msghdr message = {};
    message.msg_iov = &vector;
    message.msg_iovlen = 1u;
    message.msg_control = control;
    message.msg_controllen = sizeof control;
    ssize_t got = -1;
    do {
        got = recvmsg(socket_, &message, MSG_CMSG_CLOEXEC);
    } while (got < 0 && errno == EINTR);
    if (got <= 0) return Status::not_found;
    for (cmsghdr *header = CMSG_FIRSTHDR(&message); header != nullptr; header = CMSG_NXTHDR(&message, header)) {
        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS) continue;
        const uint32_t carried = static_cast<uint32_t>((header->cmsg_len - CMSG_LEN(0)) / sizeof(int));
        for (uint32_t index = 0u; index < carried; index++) {
            int descriptor = -1;
            memcpy(&descriptor, CMSG_DATA(header) + index * sizeof(int), sizeof descriptor);
            if (count < max_descriptors) descriptors[count++] = descriptor;
            else close(descriptor);
        }
    }
    if ((message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0) {
        for (uint32_t index = 0u; index < count; index++) close(descriptors[index]);
        count = 0u;
        return Status::too_large;
    }
    length = static_cast<size_t>(got);
    return Status::ok;
}

Status PosixRendererChannel::send(const uint8_t *bytes, size_t length, const int *descriptors, uint32_t count)
{
    AV_SCOPE("PosixRendererChannel::send");
    if (count > max_descriptors || (count != 0u && descriptors == nullptr)) return Status::invalid_parameter;
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * max_descriptors)] = {};
    iovec  vector = {const_cast<uint8_t *>(bytes), length};
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
        memcpy(CMSG_DATA(header), descriptors, sizeof(int) * count);
    }
    ssize_t sent = -1;
    do {
        sent = sendmsg(socket_, &message, MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);
    return sent == static_cast<ssize_t>(length) ? Status::ok : Status::not_found;
}

}  // namespace av
