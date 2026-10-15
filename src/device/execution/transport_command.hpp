/* One command of API 0, the transport's own (docs/protocol.md, "Execution"),
 * which the device answers itself rather than handing to the executor
 * library: a NOP runs and its answer is empty, whichever library is loaded,
 * the blob pair imports a context's guest memory into its executor, and the
 * shared-object trio gives one host object an identity the whole device
 * agrees on (feature 053), and ANSWER_PIECE hands on part of an answer too
 * long for the reply slot. PIECE never arrives here: the batch gathers it. */
#ifndef AV_TRANSPORT_COMMAND_HPP
#define AV_TRANSPORT_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobImports;
class ContextExecutor;

class TransportCommand {
public:
    /* `body` is what follows the header, as the stream framed it. */
    TransportCommand(BlobImports &imports, ContextExecutor &executor, const CommandHeader &header,
                     const uint8_t *body);

    /* Worker. ok for a NOP, whose answer is empty; malformed for a NOP of any
     * size but its header, since no producer frames a body on it; the blob
     * command's answer for IMPORT_BLOB and RELEASE_BLOB; not_found for any
     * other transport id, which the batch skips as it skips an id the executor
     * does not know. `produced` and `reply` as the executor's are. */
    [[nodiscard]] Status run(uint8_t *reply, size_t capacity, size_t &produced);

private:
    BlobImports     &imports_;
    ContextExecutor &executor_;
    CommandHeader  header_;
    const uint8_t *body_;
};

}  // namespace av

#endif  // AV_TRANSPORT_COMMAND_HPP
