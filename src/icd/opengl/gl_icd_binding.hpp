/* The one pointer the generated OpenGL entries' hooks and the ICD's own entry
 * points reach the root through (constitution III): a hook is a free function
 * the emitter calls by name, and an entry point is a bare pointer opengl32 or
 * an application calls with OpenGL's arguments only. GlIcdRoot binds itself
 * when constructed and unbinds when destroyed. */
#ifndef AV_GLICD_GL_ICD_BINDING_HPP
#define AV_GLICD_GL_ICD_BINDING_HPP

namespace av::glicd {

class GlIcdRoot;

class GlIcdBinding {
public:
    void       bind(GlIcdRoot &root);
    void       unbind();
    bool       bound() const;
    GlIcdRoot &root() const;

private:
    GlIcdRoot *root_ = nullptr;
};

extern GlIcdBinding gl_icd_binding;

}  // namespace av::glicd

#endif  // AV_GLICD_GL_ICD_BINDING_HPP
