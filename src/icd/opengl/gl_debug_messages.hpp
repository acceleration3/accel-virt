#ifndef AV_GLICD_GL_DEBUG_MESSAGES_HPP
#define AV_GLICD_GL_DEBUG_MESSAGES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::glicd {

class GlDebugMessages {
public:
    static constexpr uint32_t debug_output = 0x92E0u;
    static constexpr uint32_t debug_output_synchronous = 0x8242u;
    static constexpr uint32_t most_per_delivery = 64u;
    static constexpr size_t   text_bytes = 1024u;

    explicit GlDebugMessages(bool output);

    void     set_callback(uint64_t function, uint64_t user);
    uint64_t function() const;
    uint64_t user() const;

    bool tracks(uint32_t capability) const;
    void enable(uint32_t capability, bool on);

    bool listening() const;
    bool synchronous() const;
    void deliver(ClientTransport &wire);

    bool invariants_hold() const;

private:
    uint64_t function_;
    uint64_t user_;
    bool     output_;
    bool     synchronous_;
    bool     delivering_;
};

}

#endif
