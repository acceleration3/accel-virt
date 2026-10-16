/* A Direct3D 11 texture made on an avgpu adapter whose host memory is laid
 * out as rows, for an API of ours that reads that memory as rows - a Vulkan or
 * OpenGL window's chain slot, an OpenCL image's twin - with the NT handle that
 * names it and the identity its private driver data was published under.
 *
 * The identity and the bytes are what the miniport recorded for the texture
 * (SharedAllocationData), read back through the handle: the size an open
 * names is the one the texture was published as, never worked out again here.
 * Translation only. */
#ifndef AV_UM_SHARED_ROWS_TEXTURE_HPP
#define AV_UM_SHARED_ROWS_TEXTURE_HPP

#include <d3d11.h>

#include <stdint.h>

namespace av {

class Kmt;

class SharedRowsTexture {
public:
    explicit SharedRowsTexture(Kmt &kmt);
    /* The handle closed and the texture released, unless taken. */
    ~SharedRowsTexture();
    SharedRowsTexture(const SharedRowsTexture &) = delete;
    SharedRowsTexture &operator=(const SharedRowsTexture &) = delete;

    /* A texture as `shape` describes, shared by NT handle, made on `device`
     * and identified through `kmt_device`, a D3DKMT device on its adapter.
     * False, with nothing made, when Direct3D will not make or share it or
     * the miniport recorded no identity for it. */
    [[nodiscard]] bool make(ID3D11Device &device, uint32_t kmt_device, const D3D11_TEXTURE2D_DESC &shape);
    /* The texture, with its reference now the caller's; null unless made. */
    ID3D11Texture2D   *take_texture();
    /* The NT handle, now the caller's to close; 0 unless made. */
    uint64_t           take_handle();
    /* What the texture was published as: its shared object's id and the
     * bytes of its memory, whole pages. */
    uint64_t           object_id() const;
    uint64_t           bytes() const;

private:
    /* The identity recorded for the texture `handle_` names, read through
     * `kmt_device`. */
    [[nodiscard]] bool identify(uint32_t kmt_device);
    void               drop();

    Kmt             &kmt_;
    ID3D11Texture2D *texture_;
    uint64_t         handle_;
    uint64_t         object_id_;
    uint64_t         bytes_;
};

}  // namespace av

#endif  // AV_UM_SHARED_ROWS_TEXTURE_HPP
