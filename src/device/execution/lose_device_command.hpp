#ifndef AV_LOSE_DEVICE_COMMAND_HPP
#define AV_LOSE_DEVICE_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextExecutor;

class LoseDeviceCommand {
public:
    LoseDeviceCommand(ContextExecutor &executor, const CommandHeader &header);

    [[nodiscard]] Status run(uint8_t *reply, size_t capacity, size_t &produced);

private:
    ContextExecutor &executor_;
    CommandHeader    header_;
};

}

#endif
