#ifndef AV_UM_LOADED_OPENGL32_HPP
#define AV_UM_LOADED_OPENGL32_HPP

#include "icd/opengl/system_opengl.hpp"

namespace av {

class LoadedOpengl32 final : public glicd::SystemOpengl {
public:
    constexpr LoadedOpengl32() = default;

    void *create_context(void *dc) override;

private:
    const char *export_ = "wglCreateContext";
};

}

#endif
