#ifndef AV_GL_DISPLAY_HPP
#define AV_GL_DISPLAY_HPP

#include <stdint.h>

namespace av {

class GlContextAttributes;

class GlDisplay {
public:

    GlDisplay();
    ~GlDisplay();
    GlDisplay(const GlDisplay &) = delete;
    GlDisplay &operator=(const GlDisplay &) = delete;

    [[nodiscard]] bool open(const uint8_t *uuid);
    void               close();
    bool               open() const;

    void *create_context(const GlContextAttributes &attributes, void *share) const;
    void *handle() const;
    [[nodiscard]] bool make_current(void *context) const;
    void  destroy_context(void *context) const;
    void  release_current() const;

private:
    void *display_;
};

}

#endif
