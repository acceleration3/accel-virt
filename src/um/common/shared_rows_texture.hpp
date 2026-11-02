#ifndef AV_UM_SHARED_ROWS_TEXTURE_HPP
#define AV_UM_SHARED_ROWS_TEXTURE_HPP

#include <d3d11.h>

#include <stdint.h>

namespace av {

class Kmt;

class SharedRowsTexture {
public:
    explicit SharedRowsTexture(Kmt &kmt);
    ~SharedRowsTexture();
    SharedRowsTexture(const SharedRowsTexture &) = delete;
    SharedRowsTexture &operator=(const SharedRowsTexture &) = delete;

    [[nodiscard]] bool make(ID3D11Device &device, uint32_t kmt_device, const D3D11_TEXTURE2D_DESC &shape);
    ID3D11Texture2D   *take_texture();
    uint64_t           take_handle();
    uint64_t           object_id() const;
    uint64_t           bytes() const;

private:
    [[nodiscard]] bool identify(uint32_t kmt_device);
    void               drop();

    Kmt             &kmt_;
    ID3D11Texture2D *texture_;
    uint64_t         handle_;
    uint64_t         object_id_;
    uint64_t         bytes_;
};

}

#endif
