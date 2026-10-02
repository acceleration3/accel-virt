/* One recorded context and the API half of the command ids it sends, learned
 * from the setup record that bound it. It is what lets a reader of a recording
 * say that a reply, a fence or a fault belongs to a Vulkan context, since only
 * a batch's commands carry an API in their own ids. */
#ifndef AV_CONTEXT_API_HPP
#define AV_CONTEXT_API_HPP

#include "device/execution/command_api.hpp"
#include <stdint.h>

namespace av {

struct ContextApiProbe;

class ContextApi {
public:
    /* No sub-API this protocol knows. */
    static constexpr uint16_t unknown_api = CommandApi::none;

    ContextApi();

    /* The context speaks the commands of `api` from now on. */
    void     learn(uint32_t context, uint16_t api);
    bool     known() const;
    bool     names(uint32_t context) const;
    uint16_t api() const;

    bool invariants_hold() const;

private:
    uint32_t context_;
    uint16_t api_;
    bool     known_;

    friend struct ContextApiProbe;
};

}  // namespace av

#endif  // AV_CONTEXT_API_HPP
