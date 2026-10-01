/* A Windows command line as avinspect.exe gets it from GetCommandLineW: the
 * program's name, then arguments split at spaces and tabs, a double-quoted
 * run kept whole. Every argument must be printable ASCII - the commands, the
 * numbers and the C:\av paths the runner passes - and anything else refuses
 * the whole line. */
#ifndef AV_COMMAND_LINE_HPP
#define AV_COMMAND_LINE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct CommandLineProbe;

class CommandLine {
public:
    static constexpr uint32_t max_arguments = 8u;
    static constexpr size_t   max_bytes = 1024u;

    CommandLine();
    CommandLine(const CommandLine &) = delete;
    CommandLine &operator=(const CommandLine &) = delete;

    /* The arguments after the program's name. malformed for a unit outside
     * printable ASCII, an unclosed quote, more than max_arguments arguments or
     * more than max_bytes characters in the whole line; nothing is kept then. */
    [[nodiscard]] Status parse(const uint16_t *line);

    uint32_t    count() const;
    /* The `index`th argument, or the empty string past the last. */
    const char *argument(uint32_t index) const;
    /* The `index`th argument as a decimal u32; false for anything else. */
    bool        number(uint32_t index, uint32_t &value) const;

    bool invariants_hold() const;

private:
    void clear();

    /* The characters, a terminator per word and the last one. */
    char     text_[max_bytes + max_arguments + 2u];
    uint32_t starts_[max_arguments];
    uint32_t count_;

    friend struct CommandLineProbe;
};

}  // namespace av

#endif  // AV_COMMAND_LINE_HPP
