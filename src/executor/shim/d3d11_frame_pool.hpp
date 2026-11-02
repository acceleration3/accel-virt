#ifndef AV_D3D11_FRAME_POOL_HPP
#define AV_D3D11_FRAME_POOL_HPP

#include "executor/d3d/frame_claim.hpp"
#include "executor/d3d/frame_pool.hpp"
#include "executor/d3d/staged_frame.hpp"

#include <stdint.h>

struct ID3D11Texture2D;

namespace av {

class D3d11HostDevice;

class D3d11FramePool {
public:

    explicit D3d11FramePool(D3d11HostDevice &device);
    ~D3d11FramePool();
    D3d11FramePool(const D3d11FramePool &) = delete;
    D3d11FramePool &operator=(const D3d11FramePool &) = delete;

    StagedFrame *stage(ID3D11Texture2D *source, uint32_t subresource, uint32_t width, uint32_t height,
                       uint32_t format);
    void         clear();

private:
    class Frame final : public StagedFrame {
    public:
        Frame();
        ~Frame();

        void    bind(D3d11HostDevice &device);
        bool    fits(uint32_t width, uint32_t height, uint32_t format) const;
        bool    take(ID3D11Texture2D *source, uint32_t subresource, uint32_t width, uint32_t height,
                     uint32_t format);
        void    drop();
    private:
        Reading map_when_copied(const uint8_t *&pixels, uint32_t &row_pitch) override;
        void    unmap() override;
        bool    copy_to_staging();

        D3d11HostDevice *device_;
        ID3D11Texture2D *held_;
        ID3D11Texture2D *texture_;
        bool             staged_;
        uint32_t         format_;
        bool             mapped_;
    };

    FramePool<Frame> frames_;
};

}

#endif
