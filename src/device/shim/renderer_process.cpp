#include "device/shim/renderer_process.hpp"

#include "core/trace/function_scope.hpp"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>

namespace av {

RendererProcess::RendererProcess()
    : memory_(), doorbells_(), host_(memory_, doorbells_), service_(host_, host_, memory_, doorbells_),
      channel_(inherited_socket), datagram_{}, answer_{}
{
    AV_SCOPE("RendererProcess::RendererProcess");
}

/* A renderer outliving QEMU would hold the GPU and guest memory for nobody:
 * it dies with its parent, and one whose parent already died before the
 * request took effect leaves at once. */
int RendererProcess::run()
{
    AV_SCOPE("RendererProcess::run");
    constexpr pid_t init = 1;
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || getppid() == init) return 1;
    if (host_.become_main_loop() != Status::ok) return 1;
    for (;;) {
        pollfd watched[2] = {{channel_.descriptor(), POLLIN, 0}, {host_.main_loop_wakeup(), POLLIN, 0}};
        if (poll(watched, 2u, -1) < 0) {
            if (errno == EINTR) continue;
            return 1;
        }
        if ((watched[1].revents & POLLIN) != 0) host_.run_main_loop_work();
        if (watched[0].revents != 0 && !serve_message()) break;
    }
    service_.shut_down();
    return 0;
}

/* A message the service refuses is logged and answered with nothing; QEMU's
 * wait for an answer that never comes is what ends a renderer QEMU no longer
 * trusts. */
bool RendererProcess::serve_message()
{
    AV_SCOPE("RendererProcess::serve_message");
    int          descriptors[PosixRendererChannel::max_descriptors] = {};
    uint32_t     count = 0u;
    size_t       length = 0u;
    const Status received = channel_.receive(datagram_, sizeof datagram_, length, descriptors, count);
    if (received == Status::not_found) return false;
    if (received != Status::ok) {
        host_.log("a message longer than any QEMU sends was refused");
        return true;
    }
    size_t       answer_length = 0u;
    int          answer_descriptor = -1;
    const Status served =
        service_.receive(datagram_, length, descriptors, count, answer_, sizeof answer_, answer_length, answer_descriptor);
    if (served != Status::ok && answer_length == 0u) host_.log("a message from QEMU was refused");
    const uint32_t attached = answer_descriptor >= 0 ? 1u : 0u;
    return answer_length == 0u || channel_.send(answer_, answer_length, &answer_descriptor, attached) == Status::ok;
}

}  // namespace av
