/* The opengl32.dll that loaded this ICD, as the ICD calls back into it: its
 * wglCreateContext, looked up in the module already loaded. */
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

}  // namespace av

#endif  // AV_UM_LOADED_OPENGL32_HPP
