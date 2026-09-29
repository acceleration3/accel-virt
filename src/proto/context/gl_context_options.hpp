/* What an OpenGL context is made with (feature 050, contracts/context_options.md):
 * a version, a profile, the WGL_ARB_create_context flags and the context whose
 * objects it shares. All zero is the host's default context, sharing nothing.
 * The share context is a D3DKMT handle in the record a process writes and a
 * device context id in the setup the miniport writes; this carries either. It
 * is decoded from bytes the other side wrote and judged by valid(). */
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
    /* The same options sharing `share_context` instead. */
    GlContextOptions sharing(uint32_t share_context) const;

    /* Every field zero. */
    bool defaults() const;
    /* Every field zero but the share context: all an OpenCL context's record
     * carries, naming the OpenGL context whose objects it shares
     * (cl_khr_gl_sharing). */
    bool shares_only() const;
    /* A version OpenGL has (1.0-1.5, 2.0-2.1, 3.0-3.3, 4.0-4.6), a defined
     * profile and only the defined flags; or the defaults. */
    bool valid() const;

    /* Its eight bytes: major, minor, profile, flags, then the share context. */
    void write(WireWriter &writer) const;
    /* Reads eight bytes, whatever they hold; valid() judges them. */
    void read(WireReader &reader);

private:
    static constexpr uint8_t defined_flags = debug | forward_compatible | robust_access | lose_context_on_reset;

    uint8_t  major_;
    uint8_t  minor_;
    uint8_t  profile_;
    uint8_t  flags_;
    uint32_t share_context_;
};

}  // namespace av

#endif  // AV_GL_CONTEXT_OPTIONS_HPP
