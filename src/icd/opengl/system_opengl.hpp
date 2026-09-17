/* The system opengl32.dll, as the ICD calls into it (research.md §4): its own
 * wglCreateContext, through which wglCreateContextAttribsARB gets the HGLRC an
 * application may hand opengl32 - opengl32 then asks the ICD for the context
 * with DrvCreateLayerContext, which answers the one already made. The Windows
 * shim looks the export up in the loaded opengl32.dll; FakeOpengl32 plays it in
 * tests. */
#ifndef AV_GLICD_SYSTEM_OPENGL_HPP
#define AV_GLICD_SYSTEM_OPENGL_HPP

namespace av::glicd {

class SystemOpengl {
public:
    SystemOpengl(const SystemOpengl &) = delete;
    SystemOpengl &operator=(const SystemOpengl &) = delete;

    /* opengl32's HGLRC for a context on `dc`; null when it made none. */
    virtual void *create_context(void *dc) = 0;

protected:
    SystemOpengl() = default;
    ~SystemOpengl() = default;
};

}  // namespace av::glicd

#endif  // AV_GLICD_SYSTEM_OPENGL_HPP
