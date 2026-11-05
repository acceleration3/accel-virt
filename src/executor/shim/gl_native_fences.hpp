#ifndef AV_GL_NATIVE_FENCES_HPP
#define AV_GL_NATIVE_FENCES_HPP

namespace av {

class GlContext;

class GlNativeFences {
public:
    GlNativeFences();
    GlNativeFences(const GlNativeFences &) = delete;
    GlNativeFences &operator=(const GlNativeFences &) = delete;

    void configure(const GlContext &context);
    void give_back();
    bool available() const;

    [[nodiscard]] int make() const;

private:
    bool lists(const char *wanted) const;

    const char *extensions_;
    void *display_;
    void *(*create_)(void *, unsigned, const int *);
    unsigned (*destroy_)(void *, void *);
    int (*duplicate_)(void *, void *);
    void (*flush_)();
};

}

#endif
