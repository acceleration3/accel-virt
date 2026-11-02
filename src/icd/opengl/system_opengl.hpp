#ifndef AV_GLICD_SYSTEM_OPENGL_HPP
#define AV_GLICD_SYSTEM_OPENGL_HPP

namespace av::glicd {

class SystemOpengl {
public:
    SystemOpengl(const SystemOpengl &) = delete;
    SystemOpengl &operator=(const SystemOpengl &) = delete;

    virtual void *create_context(void *dc) = 0;

protected:
    SystemOpengl() = default;
    ~SystemOpengl() = default;
};

}

#endif
