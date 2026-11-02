#ifndef AV_COMMAND_API_HPP
#define AV_COMMAND_API_HPP

#include "proto/context/sub_api.hpp"

#include <stdint.h>

namespace av {

class CommandApi {
public:
    static constexpr uint16_t none = 0xFFFFu;

    explicit CommandApi(uint16_t api);
    explicit CommandApi(SubApi sub_api);

    [[nodiscard]] bool take_name(const char *name);

    uint16_t    api() const;
    bool        known() const;
    const char *name() const;
    const char *command_name(uint32_t id) const;

private:
    uint16_t api_;
};

}

#endif
