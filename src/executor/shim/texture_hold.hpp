#ifndef AV_TEXTURE_HOLD_HPP
#define AV_TEXTURE_HOLD_HPP

#include <stdint.h>

namespace av {

class ExportableImages;

class TextureHold {
public:
    explicit TextureHold(ExportableImages &owner);
    TextureHold(const TextureHold &) = delete;
    TextureHold &operator=(const TextureHold &) = delete;

    virtual int32_t  query_interface(const void *iid, void **object);
    virtual uint32_t add_ref();
    virtual uint32_t release();

    bool retired() const;

protected:
    ~TextureHold() = default;

private:
    ExportableImages *owner_;
    uint32_t          references_;
    bool              held_;
};

}

#endif
