/* A command that crossed in pieces, gathered back into one payload
 * (docs/protocol.md, "Long commands"): each piece's body appended in turn,
 * then the command itself closes it with the payload's last bytes. The
 * memory grows with what arrives - the guest's room and ring bound a piece,
 * not the command - and goes back once the command has run. A context's
 * worker gathers its commands, and a recording's replay its. */
#ifndef AV_GATHERED_COMMAND_HPP
#define AV_GATHERED_COMMAND_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class Host;
struct GatheredCommandProbe;

class GatheredCommand {
public:
    explicit GatheredCommand(Host &host);
    ~GatheredCommand();
    GatheredCommand(const GatheredCommand &) = delete;
    GatheredCommand &operator=(const GatheredCommand &) = delete;

    /* A piece's `length` bytes appended. no_memory, with what was gathered
     * given back, when the host has none for them. */
    [[nodiscard]] Status gather(const uint8_t *body, size_t length);
    /* Whether pieces wait for their command. */
    bool                 gathering() const;
    /* The command's own `length` bytes appended, and the whole payload in
     * `payload` and `size`, valid until clear. no_memory as gather. */
    [[nodiscard]] Status close(const uint8_t *body, size_t length, const uint8_t *&payload, size_t &size);
    /* What was gathered, given back. */
    void                 clear();

    bool invariants_hold() const;

private:
    [[nodiscard]] Status append(const uint8_t *body, size_t length);

    Host    &host_;
    uint8_t *block_;
    size_t   capacity_;
    size_t   used_;

    friend struct GatheredCommandProbe;
};

}  // namespace av

#endif  // AV_GATHERED_COMMAND_HPP
