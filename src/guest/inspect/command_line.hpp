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

    [[nodiscard]] Status parse(const uint16_t *line);

    uint32_t    count() const;
    const char *argument(uint32_t index) const;
    bool        number(uint32_t index, uint32_t &value) const;

    bool invariants_hold() const;

private:
    void clear();

    char     text_[max_bytes + max_arguments + 2u];
    uint32_t starts_[max_arguments];
    uint32_t count_;

    friend struct CommandLineProbe;
};

}

#endif
