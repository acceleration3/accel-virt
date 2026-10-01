/* What a Direct3D session does with a command it refused - one that did not
 * decode, or whose call the host's runtime would not make. It removes the
 * device that sent it, not the session: the session is every device of a
 * guest process, the whole of the desktop's composition in DWM's case. A
 * command the guest waits on is answered with nothing - its decoder writes an
 * answer only once the call succeeded - and every generated client reads an
 * empty answer as a failed call; the guest learns why at the device's next
 * Finish and recreates it, as it would after a TDR. Only a refusal with no
 * device to blame fails the session.
 *
 * Direct3D 9's, 11's and 12's sessions all refuse this way; each keeps its
 * own removed devices. */
#ifndef AV_COMMAND_REFUSAL_HPP
#define AV_COMMAND_REFUSAL_HPP

#include <stdint.h>

namespace av {

class CommandRefusal {
public:
    /* `api` names the session in what it logs ("d3d9"). */
    explicit CommandRefusal(const char *api);
    CommandRefusal(const CommandRefusal &) = delete;
    CommandRefusal &operator=(const CommandRefusal &) = delete;

    /* Whether the refused `command` removes `device` rather than failing the
     * session: false for device 0. `removed` says the device already was:
     * the first refusal says why, every later one follows from it, so only
     * the first is logged. */
    [[nodiscard]] bool removes(const char *command, uint32_t device, bool removed) const;

private:
    const char *api_;
};

}  // namespace av

#endif  // AV_COMMAND_REFUSAL_HPP
