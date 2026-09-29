/* The tests' GlShareSource: one OpenGL context the test says is current,
 * with the share name it says the OpenGL ICD gives it; every other context is
 * nobody's. It counts the questions, so a test can tell a context made
 * without asking from one that asked and was refused. */
#ifndef AV_SCRIPTED_GL_SHARE_SOURCE_HPP
#define AV_SCRIPTED_GL_SHARE_SOURCE_HPP

#include "icd/opencl/gl_share_source.hpp"

#include <cstdint>

namespace av {

class ScriptedGlShareSource final : public clicd::GlShareSource {
public:
    ScriptedGlShareSource();

    /* `gl_context` is current from now on, and named `name`. */
    void          make_current(void *gl_context, std::uint32_t name);
    std::uint32_t share_name(void *gl_context) override;
    std::uint32_t asked() const;

private:
    void         *current_;
    std::uint32_t name_;
    std::uint32_t asked_;
};

}  // namespace av

#endif  // AV_SCRIPTED_GL_SHARE_SOURCE_HPP
