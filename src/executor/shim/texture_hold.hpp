/* What a Direct3D 11 texture of DXVK's holds as private data so the Vulkan
 * image under it outlives it: a COM object as DXVK sees one, which counts its
 * references and tells ExportableImages when the last goes. Its three
 * virtual functions are IUnknown's, in IUnknown's order and with its native
 * types (DXVK's native build calls them with the platform's own convention),
 * which is all its vtable is - declared without IUnknown, whose header this
 * one must not pull in.
 *
 * DXVK lets go of it as it destroys the texture, on whichever thread gave
 * back the texture's last reference, so the count is atomic. */
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

    /* Held by a texture once, and let go of by it. */
    bool retired() const;

protected:
    /* Nothing is destroyed through the vtable DXVK holds, only as the image
     * that derives from this. */
    ~TextureHold() = default;

private:
    ExportableImages *owner_;
    uint32_t          references_;
    bool              held_;
};

}  // namespace av

#endif  // AV_TEXTURE_HOLD_HPP
