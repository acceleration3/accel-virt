#ifndef AV_SCRIPTED_GL_SHARE_SOURCE_HPP
#define AV_SCRIPTED_GL_SHARE_SOURCE_HPP

#include "icd/opencl/gl_share_source.hpp"

#include <cstdint>

namespace av {

class ScriptedGlShareSource final : public clicd::GlShareSource {
public:
    ScriptedGlShareSource();

    void          make_current(void *gl_context, std::uint32_t name, std::uint64_t luid = 0u);
    std::uint32_t share_name(void *gl_context) override;
    std::uint64_t adapter_luid(void *gl_context) override;
    void          run_held_calls() override;
    std::uint32_t asked() const;
    std::uint32_t runs() const;
    std::uint32_t commands_at_run() const;
    void          count_commands_with(std::uint32_t (*count)(void *), void *counter);

private:
    void         *current_;
    std::uint32_t name_;
    std::uint64_t luid_;
    std::uint32_t asked_;
    std::uint32_t runs_;
    std::uint32_t commands_at_run_;
    std::uint32_t (*count_)(void *);
    void         *counter_;
};

}

#endif
