#ifndef AV_COMMAND_REFUSAL_HPP
#define AV_COMMAND_REFUSAL_HPP

#include <stdint.h>

namespace av {

class DriverLog;

class CommandRefusal {
public:
    explicit CommandRefusal(DriverLog &lines);
    CommandRefusal(const CommandRefusal &) = delete;
    CommandRefusal &operator=(const CommandRefusal &) = delete;

    [[nodiscard]] bool removes(const char *command, uint32_t device, bool removed) const;

private:
    DriverLog &lines_;
};

}

#endif
