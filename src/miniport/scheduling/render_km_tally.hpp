#ifndef AV_RENDER_KM_TALLY_HPP
#define AV_RENDER_KM_TALLY_HPP

#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DeviceTable;
class StateWriter;
struct DxgkRenderkmCommandRecord;
struct DxgkargRenderRecord;
struct RenderKmTallyProbe;

class RenderKmTally {
public:
    RenderKmTally(Platform &platform, DeviceTable &devices);
    RenderKmTally(const RenderKmTally &) = delete;
    RenderKmTally &operator=(const RenderKmTally &) = delete;

    [[nodiscard]] NtStatus render(DxgkargRenderRecord &request);

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t no_op_bytes = 4u;
    static constexpr uint32_t insufficient_dma_buffer = 0xC01E0001u;

    [[nodiscard]] Status walk(const DxgkargRenderRecord &request);
    void                 count(const DxgkRenderkmCommandRecord &command, uint32_t available,
                               const DxgkargRenderRecord &request);
    uint32_t             kind_at(const DxgkargRenderRecord &request, uint32_t index);

    Platform    &platform_;
    DeviceTable &devices_;
    uint64_t     calls_;
    uint64_t     bitblts_;
    uint64_t     color_fills_;
    uint64_t     alpha_blends_;
    uint64_t     stretch_blts_;
    uint64_t     transparent_blts_;
    uint64_t     cleartype_blends_;
    uint64_t     escapes_;
    uint64_t     unknown_;
    uint32_t     last_bitblt_source_kind_;
    uint32_t     last_bitblt_destination_kind_;

    friend struct RenderKmTallyProbe;
};

}

#endif
