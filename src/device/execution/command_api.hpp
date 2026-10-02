/* Which API a command belongs to, as the wire names it twice over: the SubApi
 * bit a context is set up for, and the API half of every command id it sends
 * (docs/protocol.md, Execution) - with the name a person types or reads for
 * each, and each API's registry for its commands' names. One table, so the
 * executor, a recording's replay, its printer and its filter cannot disagree:
 * the copies they each kept had all forgotten Direct3D.
 *
 * Direct3D 9, 11 and 12 are one API on the wire, each runtime's ids in a range
 * of its own, so a Direct3D id's name is whichever of the three registries
 * has it. */
#ifndef AV_COMMAND_API_HPP
#define AV_COMMAND_API_HPP

#include "proto/context/sub_api.hpp"

#include <stdint.h>

namespace av {

class CommandApi {
public:
    /* No API: what a SubApi bit that is none, or a name that is none, gives. */
    static constexpr uint16_t none = 0xFFFFu;

    /* A command id's API half. */
    explicit CommandApi(uint16_t api);
    /* What a context set up for `sub_api` sends; none for a bit that is no
     * API. */
    explicit CommandApi(SubApi sub_api);

    /* The API `name` names: transport, vulkan, opencl, opengl or d3d. False,
     * and nothing changed, for any other. */
    [[nodiscard]] bool take_name(const char *name);

    uint16_t    api() const;
    bool        known() const;
    /* Its name, or "api" for one this table does not have; never null. */
    const char *name() const;
    /* The registry's name for command `id` of this API, or the transport's
     * own; "unknown" for an id no table has, never null. */
    const char *command_name(uint32_t id) const;

private:
    uint16_t api_;
};

}  // namespace av

#endif  // AV_COMMAND_API_HPP
