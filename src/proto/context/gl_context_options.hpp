#ifndef AV_GL_CONTEXT_OPTIONS_HPP
#define AV_GL_CONTEXT_OPTIONS_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class GlContextOptions {
public:
    enum class Profile : uint8_t { none = 0u, core = 1u, compatibility = 2u };

    static constexpr size_t encoded_size = 8u;

    static constexpr uint8_t debug                 = 1u << 0u;
    static constexpr uint8_t forward_compatible    = 1u << 1u;
    static constexpr uint8_t robust_access         = 1u << 2u;
    static constexpr uint8_t lose_context_on_reset = 1u << 3u;

    GlContextOptions();
    GlContextOptions(uint8_t major, uint8_t minor, Profile profile, uint8_t flags, uint32_t share_context);

    uint8_t  major() const;
    uint8_t  minor() const;
    Profile  profile() const;
    uint8_t  flags() const;
    uint32_t share_context() const;
    GlContextOptions sharing(uint32_t share_context) const;

    bool defaults() const;
    bool shares_only() const;
    bool valid() const;
    bool fit_for(uint32_t sub_api) const;

    void write(WireWriter &writer) const;
    void read(WireReader &reader);

private:
    static constexpr uint8_t defined_flags = debug | forward_compatible | robust_access | lose_context_on_reset;

    uint8_t  major_;
    uint8_t  minor_;
    uint8_t  profile_;
    uint8_t  flags_;
    uint32_t share_context_;
};

}

#endif
