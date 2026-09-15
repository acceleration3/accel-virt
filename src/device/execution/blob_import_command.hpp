/* IMPORT_BLOB or RELEASE_BLOB, the transport commands that hand a context's
 * guest blob to its executor and take it back (docs/protocol.md, Transport
 * commands; feature 045). A body no producer frames is command framing, which
 * faults the context as a NOP with a body does; every other refusal is an
 * answer, and the context runs on. */
#ifndef AV_BLOB_IMPORT_COMMAND_HPP
#define AV_BLOB_IMPORT_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobImports;

class BlobImportCommand {
public:
    static constexpr size_t release_body_size = 8u;

    /* `body` is the header's size less the header's bytes, as the stream framed it. */
    BlobImportCommand(BlobImports &imports, const CommandHeader &header, const uint8_t *body);

    /* Worker. malformed for a body of any length but its command's; not_found
     * for any other transport id; ok otherwise, with `produced` the answer's
     * length - 16 for an import, 0 for a release - and the answer in `reply`
     * when `capacity` holds it. */
    [[nodiscard]] Status run(uint8_t *reply, size_t capacity, size_t &produced);

private:
    [[nodiscard]] Status import(uint8_t *reply, size_t capacity, size_t &produced);
    [[nodiscard]] Status release();
    size_t               body_size() const;

    BlobImports   &imports_;
    CommandHeader  header_;
    const uint8_t *body_;
};

}  // namespace av

#endif  // AV_BLOB_IMPORT_COMMAND_HPP
