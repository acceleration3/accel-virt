/* What a Direct3D session does with a command it refused - one that did not
 * decode, or whose call the host's runtime would not make. A command whose
 * guest waits on the answer fails the session: the decoder writes its answer
 * only once the call succeeded, so there is no answer to give, and an empty
 * one is a reply the guest's decoder misreads. So does a command with no
 * device to blame. Any other is dropped and removes the device that sent it,
 * not the session - the session is every device of a guest process, the
 * whole of the desktop's composition in DWM's case. The guest learns why at
 * its next Finish and recreates the device, as it would after a TDR.
 *
 * Direct3D 9's, 11's and 12's sessions all refuse this way; each keeps its
 * own removed devices. */
#ifndef AV_COMMAND_REFUSAL_HPP
#define AV_COMMAND_REFUSAL_HPP

#include <stdint.h>

namespace av {

class WireWriter;

class CommandRefusal {
public:
    /* `api` names the session in what it logs ("d3d9"). */
    explicit CommandRefusal(const char *api);
    CommandRefusal(const CommandRefusal &) = delete;
    CommandRefusal &operator=(const CommandRefusal &) = delete;

    /* Whether the refused `command` removes `device` rather than failing the
     * session: false when its `reply` was lent room - the guest waits on it -
     * or `device` is 0. `removed` says the device already was: the first
     * refusal says why, every later one follows from it, so only the first
     * is logged. */
    [[nodiscard]] bool removes(const char *command, const WireWriter &reply, uint32_t device, bool removed) const;

private:
    const char *api_;
};

}  // namespace av

#endif  // AV_COMMAND_REFUSAL_HPP
