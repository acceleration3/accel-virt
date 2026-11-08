#ifndef AV_HOST_FORMAT_CONVERTER_HPP
#define AV_HOST_FORMAT_CONVERTER_HPP

#include "executor/shim/scrgb_rows.hpp"

#include <stdint.h>

struct ID3D11Device5;
struct ID3D11DeviceContext4;
struct ID3D11Texture2D;
struct ID3D11VideoDevice;
struct ID3D11VideoContext;
struct ID3D11VideoProcessorEnumerator;
struct ID3D11VideoProcessor;
struct ID3D11VideoProcessorOutputView;

namespace av {

class HostFormatConverter {
public:
    struct Rectangle {
        uint32_t left;
        uint32_t top;
        uint32_t right;
        uint32_t bottom;
    };

    HostFormatConverter();
    ~HostFormatConverter();
    HostFormatConverter(const HostFormatConverter &) = delete;
    HostFormatConverter &operator=(const HostFormatConverter &) = delete;

    ID3D11Texture2D *draw(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Texture2D *source,
                          uint32_t subresource, const Rectangle &from, uint32_t width, uint32_t height,
                          uint32_t target_format);
    void             release();

private:
    ID3D11Texture2D *draw_scrgb(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Texture2D *source,
                                uint32_t subresource, const Rectangle &from, uint32_t width, uint32_t height,
                                uint32_t target_format);
    [[nodiscard]] bool stage(ID3D11Device5 *device, ID3D11Texture2D *&texture, uint32_t &held_width,
                             uint32_t &held_height, uint32_t &held_format, uint32_t width, uint32_t height,
                             uint32_t format, bool writing);
    [[nodiscard]] bool prepare(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Texture2D *source,
                               uint32_t width, uint32_t height, uint32_t target_format);

    ID3D11VideoDevice              *video_;
    ID3D11VideoContext             *video_context_;
    ID3D11VideoProcessorEnumerator *enumerator_;
    ID3D11VideoProcessor           *processor_;
    ID3D11Texture2D                *target_;
    ID3D11VideoProcessorOutputView *output_;
    uint32_t                        source_width_;
    uint32_t                        source_height_;
    uint32_t                        width_;
    uint32_t                        height_;
    uint32_t                        format_;
    ScrgbRows                       rows_;
    ID3D11Texture2D                *read_;
    ID3D11Texture2D                *written_;
    uint32_t                        read_width_;
    uint32_t                        read_height_;
    uint32_t                        read_format_;
    uint32_t                        written_width_;
    uint32_t                        written_height_;
    uint32_t                        written_format_;
};

}

#endif
