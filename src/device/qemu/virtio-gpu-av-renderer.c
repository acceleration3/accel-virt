/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The renderer half of the AvHostOps table (docs/renderer.md): av-renderer
 * spawned with its end of a SOCK_SEQPACKET socketpair as descriptor 3, watched
 * through a pidfd and the socket's hangup, datagrams and their descriptors
 * carried with SCM_RIGHTS, and guest RAM described as the memfd-backed
 * RAMBlocks the renderer maps. Translation only: the core decides when a
 * renderer starts, what it is sent and what its death means.
 *
 * Installed by accel-virt's tools/build_qemu.sh as hw/display/virtio-gpu-av-renderer.c.
 */
#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"
#include "qemu/rcu.h"
#include "system/ramblock.h"
#include "system/ramlist.h"
#include "hw/virtio/virtio-gpu-av.h"

#include <glib-unix.h>
#include <poll.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#ifndef P_PIDFD
#define P_PIDFD 3
#endif

/* The descriptor the renderer finds its socket at. */
#define AV_RENDERER_SOCKET_FD 3
/* One datagram of the largest body (RendererMessageHeader::max_body_length)
 * with its 16-byte header, and the 32 bytes a unix socket's send buffer keeps
 * back from a datagram. */
#define AV_RENDERER_MAX_DATAGRAM (0x40010u + 16u)
#define AV_RENDERER_SOCKET_BUFFER (AV_RENDERER_MAX_DATAGRAM + 32u)
/* How long a send waits for room before the renderer counts as gone: one that
 * reads nothing for this long is hung, and the main loop must not be. */
#define AV_RENDERER_SEND_TIMEOUT_MS 5000
/* The most descriptors one datagram carries either way: a fence line per
 * doorbell page. */
#define AV_RENDERER_MAX_DESCRIPTORS 64u

/* What receive_renderer answers (av_device.h). */
#define AV_RENDERER_RECEIVED 0
#define AV_RENDERER_TIMED_OUT 1
#define AV_RENDERER_GONE 2

void virtio_gpu_av_renderer_init(VirtIOGPUAv *r)
{
    r->process.pid = -1;
    r->process.pidfd = -1;
    r->process.socket = -1;
    r->process.hangup = 0;
    r->process.region_count = 0;
}

/* Reaps the renderer and releases what watched it: true when there was one.
 * It is killed first, so a renderer whose socket closed while it went on
 * running cannot be waited on for ever. */
static bool av_renderer_end(VirtIOGPUAv *r)
{
    VirtIOGPUAvRenderer *p = &r->process;
    siginfo_t info = { 0 };

    if (p->pidfd < 0) {
        return false;
    }
    qemu_set_fd_handler(p->pidfd, NULL, NULL, NULL);
    if (p->hangup) {
        g_source_remove(p->hangup);
        p->hangup = 0;
    }
    syscall(SYS_pidfd_send_signal, p->pidfd, SIGKILL, NULL, 0);
    if (waitid(P_PIDFD, p->pidfd, &info, WEXITED) < 0) {
        warn_report("virtio-gpu-av: renderer %d could not be reaped: %s", (int)p->pid, strerror(errno));
    } else if (info.si_code == CLD_EXITED) {
        warn_report("virtio-gpu-av: renderer %d exited with status %d", (int)p->pid, info.si_status);
    } else {
        warn_report("virtio-gpu-av: renderer %d ended by signal %d", (int)p->pid, info.si_status);
    }
    close(p->pidfd);
    close(p->socket);
    p->pidfd = -1;
    p->socket = -1;
    p->pid = -1;
    return true;
}

/* The core is told once, whichever of the pidfd and the hangup comes first;
 * a core that failed to realize has nothing to tell. */
static void av_renderer_lost(VirtIOGPUAv *r)
{
    if (av_renderer_end(r) && r->core) {
        av_device_renderer_lost(r->core);
    }
}

static void av_renderer_exited(void *opaque)
{
    av_renderer_lost(opaque);
}

static gboolean av_renderer_hung_up(gint fd, GIOCondition condition, gpointer opaque)
{
    VirtIOGPUAv *r = opaque;

    r->process.hangup = 0;
    av_renderer_lost(r);
    return G_SOURCE_REMOVE;
}

void virtio_gpu_av_renderer_finish(VirtIOGPUAv *r)
{
    (void)av_renderer_end(r);
}

/* Both ends hold one whole datagram of the largest message; a host whose
 * net.core.wmem_max caps the buffer below that is told why sends fail. */
static void av_renderer_size_buffers(int fd)
{
    int size = AV_RENDERER_SOCKET_BUFFER;
    socklen_t length = sizeof(size);

    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, &length) == 0 && size < (int)AV_RENDERER_SOCKET_BUFFER) {
        warn_report("virtio-gpu-av: the renderer socket holds %d bytes, less than one %u-byte message; "
                    "raise net.core.wmem_max", size, AV_RENDERER_MAX_DATAGRAM);
    }
}

/* Spawns `path` with the inherited environment, default signal dispositions
 * and an empty mask (QEMU ignores SIGPIPE and blocks others, and both would
 * survive the exec), and its socket at descriptor 3. */
static int av_renderer_spawn(const char *path, int child_end, pid_t *pid)
{
    char *const argv[] = { (char *)path, NULL };
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    sigset_t signals;
    int failed;

    posix_spawn_file_actions_init(&actions);
    posix_spawnattr_init(&attributes);
    posix_spawn_file_actions_adddup2(&actions, child_end, AV_RENDERER_SOCKET_FD);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK);
    sigfillset(&signals);
    posix_spawnattr_setsigdefault(&attributes, &signals);
    sigemptyset(&signals);
    posix_spawnattr_setsigmask(&attributes, &signals);
    failed = posix_spawn(pid, path, &actions, &attributes, argv, environ);
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    return failed;
}

static int av_start_renderer(void *opaque, const char *path)
{
    VirtIOGPUAv *r = opaque;
    VirtIOGPUAvRenderer *p = &r->process;
    int ends[2];
    int child_end;
    int failed;
    pid_t pid;

    if (p->pidfd >= 0 || !path || !path[0]) {
        return 1;
    }
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, ends) < 0) {
        warn_report("virtio-gpu-av: no socket for the renderer: %s", strerror(errno));
        return 1;
    }
    av_renderer_size_buffers(ends[0]);
    av_renderer_size_buffers(ends[1]);
    /* dup2 onto itself would leave close-on-exec set, and the renderer would
     * start without its socket. */
    child_end = ends[1];
    if (child_end == AV_RENDERER_SOCKET_FD) {
        child_end = fcntl(ends[1], F_DUPFD_CLOEXEC, AV_RENDERER_SOCKET_FD + 1);
        close(ends[1]);
        if (child_end < 0) {
            close(ends[0]);
            return 1;
        }
    }
    failed = av_renderer_spawn(path, child_end, &pid);
    close(child_end);
    if (failed) {
        warn_report("virtio-gpu-av: renderer '%s' could not be started: %s", path, strerror(failed));
        close(ends[0]);
        return 1;
    }
    p->pidfd = (int)syscall(SYS_pidfd_open, pid, 0);
    if (p->pidfd < 0) {
        warn_report("virtio-gpu-av: renderer %d cannot be watched: %s", (int)pid, strerror(errno));
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        close(ends[0]);
        return 1;
    }
    p->pid = pid;
    p->socket = ends[0];
    qemu_set_fd_handler(p->pidfd, av_renderer_exited, NULL, r);
    p->hangup = g_unix_fd_add(p->socket, G_IO_HUP | G_IO_ERR, av_renderer_hung_up, r);
    return 0;
}

/* Waits for room rather than blocking in sendmsg, so a renderer that stopped
 * reading costs the main loop a bounded wait, not the VM. */
static int av_send_renderer(void *opaque, const uint8_t *bytes, size_t length, const int *descriptors,
                             uint32_t count)
{
    VirtIOGPUAv *r = opaque;
    union {
        char buffer[CMSG_SPACE(sizeof(int) * AV_RENDERER_MAX_DESCRIPTORS)];
        struct cmsghdr align;
    } control;
    struct iovec vector = { .iov_base = (void *)bytes, .iov_len = length };
    struct msghdr message = { .msg_iov = &vector, .msg_iovlen = 1 };
    struct pollfd room = { .fd = r->process.socket, .events = POLLOUT };

    if (r->process.socket < 0 || count > AV_RENDERER_MAX_DESCRIPTORS) {
        return 1;
    }
    if (count) {
        struct cmsghdr *header;

        memset(&control, 0, sizeof(control));
        message.msg_control = control.buffer;
        message.msg_controllen = CMSG_SPACE(sizeof(int) * count);
        header = CMSG_FIRSTHDR(&message);
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int) * count);
        memcpy(CMSG_DATA(header), descriptors, sizeof(int) * count);
    }
    for (;;) {
        ssize_t sent = sendmsg(r->process.socket, &message, MSG_NOSIGNAL | MSG_DONTWAIT);
        int ready;

        if (sent == (ssize_t)length) {
            return 0;
        }
        if (sent >= 0 || (errno != EAGAIN && errno != EINTR)) {
            return 1;
        }
        if (errno == EINTR) {
            continue;
        }
        do {
            ready = poll(&room, 1, AV_RENDERER_SEND_TIMEOUT_MS);
        } while (ready < 0 && errno == EINTR);
        if (ready <= 0 || (room.revents & (POLLHUP | POLLERR))) {
            return 1;
        }
    }
}

/* The descriptors of a datagram whose control data did not fit are closed by
 * the kernel past the room given; what did fit is passed on, and the core
 * refuses a message whose count is not the one its kind carries. */
static int av_receive_renderer(void *opaque, uint8_t *buffer, size_t capacity, size_t *length, int *descriptors,
                                uint32_t capacity_descriptors, uint32_t *count, uint32_t timeout_ms)
{
    VirtIOGPUAv *r = opaque;
    union {
        char buffer[CMSG_SPACE(sizeof(int) * AV_RENDERER_MAX_DESCRIPTORS)];
        struct cmsghdr align;
    } control;
    struct iovec vector = { .iov_base = buffer, .iov_len = capacity };
    struct msghdr message = { .msg_iov = &vector, .msg_iovlen = 1 };
    struct pollfd waiting = { .fd = r->process.socket, .events = POLLIN };
    uint32_t room = capacity_descriptors < AV_RENDERER_MAX_DESCRIPTORS ? capacity_descriptors
                                                                        : AV_RENDERER_MAX_DESCRIPTORS;
    ssize_t got;
    int ready;

    *length = 0;
    *count = 0;
    if (r->process.socket < 0) {
        return AV_RENDERER_GONE;
    }
    do {
        ready = poll(&waiting, 1, (int)timeout_ms);
    } while (ready < 0 && errno == EINTR);
    if (ready == 0) {
        return AV_RENDERER_TIMED_OUT;
    }
    if (ready < 0 || !(waiting.revents & POLLIN)) {
        return AV_RENDERER_GONE;
    }
    if (room) {
        memset(&control, 0, sizeof(control));
        message.msg_control = control.buffer;
        message.msg_controllen = CMSG_SPACE(sizeof(int) * room);
    }
    do {
        got = recvmsg(r->process.socket, &message, MSG_CMSG_CLOEXEC | MSG_TRUNC | MSG_DONTWAIT);
    } while (got < 0 && errno == EINTR);
    if (got < 0 && errno == EAGAIN) {
        return AV_RENDERER_TIMED_OUT;
    }
    if (got <= 0) {
        return AV_RENDERER_GONE;
    }
    for (struct cmsghdr *header = CMSG_FIRSTHDR(&message); header; header = CMSG_NXTHDR(&message, header)) {
        size_t carried;
        int passed[AV_RENDERER_MAX_DESCRIPTORS];

        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS) {
            continue;
        }
        carried = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        memcpy(passed, CMSG_DATA(header), carried * sizeof(int));
        for (size_t index = 0; index < carried; index++) {
            if (*count < room) {
                descriptors[(*count)++] = passed[index];
            } else {
                close(passed[index]);
            }
        }
    }
    /* MSG_TRUNC: the datagram's own length, past `capacity` if it was cut. */
    *length = (size_t)got;
    return AV_RENDERER_RECEIVED;
}

/* The core asks this when it has already given the renderer up: it is ended
 * and reaped here, before the core starts another, and the core is not told
 * of a loss it caused. */
static void av_kill_renderer(void *opaque)
{
    (void)av_renderer_end(opaque);
}

/*
 * Guest RAM regions. A region is a RAMBlock by its index in QEMU's RAM list
 * when the core counted them; what is remembered is the block's host address,
 * never the RAMBlock, which RCU may free.
 *
 * A block can start at a nonzero offset of its file (a memory-backend-file
 * with offset=, or blocks sharing one file), and the renderer maps a region
 * from file offset 0, since an mmap offset must be page-aligned and the file
 * is the unit it receives. So a region covers the file from 0 to the block's
 * end - fd_offset + used_length bytes - and a pointer's offset in its region
 * is its offset in the file. The bytes before the block are mapped but never
 * named by an extent QEMU encodes.
 */
typedef struct VirtIOGPUAvRegionSearch {
    void *host;
    int descriptor;
} VirtIOGPUAvRegionSearch;

static int av_note_region(RAMBlock *block, void *opaque)
{
    VirtIOGPUAv *r = opaque;
    VirtIOGPUAvRegion *region;

    if (r->process.region_count == VIRTIO_GPU_AV_MAX_REGIONS) {
        return 1;
    }
    region = &r->process.regions[r->process.region_count++];
    region->host = qemu_ram_get_host_addr(block);
    region->fd_offset = qemu_ram_get_fd_offset(block);
    region->used = qemu_ram_get_used_length(block);
    region->mappable = region->host && qemu_ram_get_fd(block) >= 0 && qemu_ram_is_shared(block);
    return 0;
}

static uint32_t av_guest_region_count(void *opaque)
{
    VirtIOGPUAv *r = opaque;

    r->process.region_count = 0;
    qemu_ram_foreach_block(av_note_region, r);
    return r->process.region_count;
}

static int av_duplicate_region(RAMBlock *block, void *opaque)
{
    VirtIOGPUAvRegionSearch *search = opaque;

    if (qemu_ram_get_host_addr(block) != search->host) {
        return 0;
    }
    if (qemu_ram_get_fd(block) >= 0 && qemu_ram_is_shared(block)) {
        search->descriptor = fcntl(qemu_ram_get_fd(block), F_DUPFD_CLOEXEC, 0);
    }
    return 1;
}

static int av_guest_region(void *opaque, uint32_t index, uint32_t *region, int *descriptor, uint64_t *size)
{
    VirtIOGPUAv *r = opaque;
    VirtIOGPUAvRegionSearch search = { .descriptor = -1 };
    const VirtIOGPUAvRegion *noted;

    *descriptor = -1;
    if (index >= r->process.region_count || !r->process.regions[index].mappable) {
        return 1;
    }
    noted = &r->process.regions[index];
    search.host = noted->host;
    qemu_ram_foreach_block(av_duplicate_region, &search);
    if (search.descriptor < 0) {
        return 1;
    }
    *region = index;
    *descriptor = search.descriptor;
    *size = noted->fd_offset + noted->used;
    return 0;
}

static int av_locate_guest(void *opaque, const void *pointer, uint64_t length, uint32_t *region, uint64_t *offset)
{
    VirtIOGPUAv *r = opaque;
    ram_addr_t at;
    RAMBlock *block;
    void *host;

    RCU_READ_LOCK_GUARD();
    block = qemu_ram_block_from_host((void *)pointer, false, &at);
    if (!block || at + length > qemu_ram_get_used_length(block)) {
        return 1;
    }
    host = qemu_ram_get_host_addr(block);
    for (uint32_t index = 0; index < r->process.region_count; index++) {
        const VirtIOGPUAvRegion *noted = &r->process.regions[index];

        if (noted->mappable && noted->host == host) {
            *region = index;
            *offset = noted->fd_offset + at;
            return 0;
        }
    }
    return 1;
}

void virtio_gpu_av_renderer_ops(AvHostOps *ops)
{
    ops->guest_region_count = av_guest_region_count;
    ops->guest_region = av_guest_region;
    ops->locate_guest = av_locate_guest;
    ops->start_renderer = av_start_renderer;
    ops->send_renderer = av_send_renderer;
    ops->receive_renderer = av_receive_renderer;
    ops->kill_renderer = av_kill_renderer;
}
