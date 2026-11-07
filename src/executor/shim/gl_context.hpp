#ifndef AV_GL_CONTEXT_HPP
#define AV_GL_CONTEXT_HPP

namespace av {

class GlContextAttributes;
class GlDisplay;

class GlContext {
public:
    GlContext();
    ~GlContext();
    GlContext(const GlContext &) = delete;
    GlContext &operator=(const GlContext &) = delete;

    [[nodiscard]] bool open(const GlDisplay &display, const GlContextAttributes &attributes, const GlContext *share);
    void               close();
    bool               open() const;

    [[nodiscard]] bool make_current();
    void               let_go();
    void              *resolve(const char *name) const;
    void              *handle() const;
    void              *display() const;

private:
    const GlDisplay *display_;
    void            *context_;
};

}

#endif
