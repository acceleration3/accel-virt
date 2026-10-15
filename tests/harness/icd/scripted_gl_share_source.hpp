/* The tests' GlShareSource: one OpenGL context the test says is current,
 * with the share name it says the OpenGL ICD gives it; every other context is
 * nobody's. It counts the questions, so a test can tell a context made
 * without asking from one that asked and was refused, and the runs of the
 * held calls, so a test can tell they came before what OpenCL sent. */
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
    void          run_held_calls() override;
    std::uint32_t asked() const;
    /* How many times the held calls were run, and the commands the device had
     * been sent at the last of them. */
    std::uint32_t runs() const;
    std::uint32_t commands_at_run() const;
    /* What counts the commands sent, read at each run. */
    void          count_commands_with(std::uint32_t (*count)(void *), void *counter);

private:
    void         *current_;
    std::uint32_t name_;
    std::uint32_t asked_;
    std::uint32_t runs_;
    std::uint32_t commands_at_run_;
    std::uint32_t (*count_)(void *);
    void         *counter_;
};

}  // namespace av

#endif  // AV_SCRIPTED_GL_SHARE_SOURCE_HPP
