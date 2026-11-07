#ifndef AV_EGL_EXTENSIONS_HPP
#define AV_EGL_EXTENSIONS_HPP

namespace av {

class EglExtensions {
public:
    EglExtensions();

    void configure(void *display);
    void forget();
    bool lists(const char *wanted) const;

private:
    const char *extensions_;
};

}

#endif
