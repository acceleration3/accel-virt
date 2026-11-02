#ifndef AV_PRESENT_REQUEST_HPP
#define AV_PRESENT_REQUEST_HPP

#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"
#include "proto/dma/present_operation.hpp"

#include <stdint.h>

namespace av {

class PresentAddressing;
struct DxgkargPresentRecord;

class PresentRequest {
public:
    PresentRequest(Platform &platform, DxgkargPresentRecord &request);
    PresentRequest(const PresentRequest &) = delete;
    PresentRequest &operator=(const PresentRequest &) = delete;

    NtStatus present();
    [[nodiscard]] Status operation(PresentOperation &operation) const;

private:
    enum : uint32_t {
        flag_blt = 0x1u,
        flag_color_fill = 0x2u,
        flag_flip = 0x4u,
        flag_flip_with_no_wait = 0x8u,
        flag_flip_with_multiplane_overlay = 0x1000u,
    };
    enum : uint32_t { source_index = 1u, destination_index = 2u };
    static constexpr uint32_t insufficient_dma_buffer = 0xC01E0001u;

    [[nodiscard]] Status validate(PresentOperation operation, const PresentAddressing &addressing) const;

    Platform             &platform_;
    DxgkargPresentRecord &request_;
};

}

#endif
