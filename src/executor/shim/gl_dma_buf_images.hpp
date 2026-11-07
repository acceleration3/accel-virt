#ifndef AV_GL_DMA_BUF_IMAGES_HPP
#define AV_GL_DMA_BUF_IMAGES_HPP

#include "core/support/keyed_slots.hpp"
#include "executor/shim/egl_extensions.hpp"
#include "executor/shim/gl_extensions.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class GlContext;
class SharedImageShape;

class GlDmaBufImages {
public:
    explicit GlDmaBufImages(BlockSource &blocks);
    GlDmaBufImages(const GlDmaBufImages &) = delete;
    GlDmaBufImages &operator=(const GlDmaBufImages &) = delete;

    void configure(GlContext &context);
    void give_back();
    bool imports_images() const;

    [[nodiscard]] bool import(uint32_t memory, int fd, const SharedImageShape &shape);
    bool               covers(uint32_t memory, uint32_t width, uint32_t height, uint32_t internal_format) const;
    [[nodiscard]] bool bind(uint32_t texture, uint32_t memory);
    void               forget(uint32_t memory);

    uint32_t count() const;

private:
    struct Image {
        void    *image_;
        uint32_t width_;
        uint32_t height_;
        uint32_t internal_format_;
    };

    void *(*create_image_)(void *, void *, unsigned, void *, const int *);
    unsigned (*destroy_image_)(void *, void *);
    void (*target_storage_)(unsigned, void *, const int *);
    EglExtensions                egl_;
    GlExtensions                 gl_;
    KeyedSlots<uint32_t, Image>  images_;
    void                        *display_;
};

}

#endif
