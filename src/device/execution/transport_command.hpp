/* One command of API 0, the transport's own (docs/protocol.md, "Execution"),
 * which the device answers itself rather than handing to the executor
 * library: a NOP runs and its answer is empty, whichever library is loaded. */
#ifndef AV_TRANSPORT_COMMAND_HPP
#define AV_TRANSPORT_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

namespace av {

class TransportCommand {
public:
    explicit TransportCommand(const CommandHeader &header);

    /* ok for a NOP, whose answer is empty; malformed for a NOP of any size but
     * its header, since no producer frames a body on it; not_found for any
     * other transport id, which the batch skips as it skips an id the
     * executor does not know. */
    [[nodiscard]] Status run() const;

private:
    CommandHeader header_;
};

}  // namespace av

#endif  // AV_TRANSPORT_COMMAND_HPP
