/* What the host GPU's Direct3D 12 device can do, asked once per adapter, and
 * every GetCaps question D3D12Core asks answered from it.
 *
 * D3D12Core asks an adapter's capabilities before it makes a device and
 * checks them hard: an options query it gets no answer to, wave lane counts
 * out of order, a shader model list without 5.1 or with gaps, memory that is
 * not IO coherent - each fails the device (d3d12core's
 * CDevice::LLOCompleteLayerConstruction). So this opens a session of its own,
 * asks the host (OpenDevice12, then QueryFormat12 per format), closes it, and
 * answers from what it kept.
 *
 * What is reported is what this driver implements over what the host has:
 * feature level 11_1 at most, since 12_0 promises tiled resources tier 2,
 * which no command here maps; multisampling only where every member of a
 * format's cast family agrees; no tiled resources, raytracing, mesh shaders,
 * view instancing or render passes. A device's CheckFormatSupport is answered from the same
 * table, so the runtime is told one story. */
#ifndef AV_UM_D3D12_HOST_CAPS_HPP
#define AV_UM_D3D12_HOST_CAPS_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"
#include "um/d3d/d3d12_ddi.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
class Kmt;
}  // namespace av

namespace av::d3d12 {

class D3d12HostCaps {
public:
    static constexpr uint32_t format_count = 192u;

    D3d12HostCaps();
    D3d12HostCaps(const D3d12HostCaps &) = delete;
    D3d12HostCaps &operator=(const D3d12HostCaps &) = delete;

    /* Asks the host through `adapter` (a D3DKMT adapter handle). False when
     * the adapter runs no Direct3D 12, which the runtime is told as a failed
     * GetCaps. */
    [[nodiscard]] bool learn(Kmt &kmt, uint32_t adapter);
    bool               learned() const;

    /* One GetCaps question, answered into the caller's buffer. */
    HRESULT answer(const D3D12DDIARG_GETCAPS &args) const;
    /* D3D12DDI_FORMAT_SUPPORT_* bits for `format`. */
    UINT    format_support(DXGI_FORMAT format) const;
    /* Quality levels of `format` at `samples`: one level at one sample for a
     * format there is at all, and at more samples a level count its whole cast
     * family shares, or none. */
    UINT    quality_levels(DXGI_FORMAT format, UINT samples) const;
    UINT    descriptor_size(uint32_t type) const;

private:
    /* 2, 4, 8 and 16 samples. */
    static constexpr uint32_t sample_counts = 4u;

    /* The host's quality levels at each count, per cast family: the fewest any
     * member the runtime lets multisample has. D3D12Core refuses a driver
     * whose family members disagree (d3d12core's LLOCompleteLayerConstruction,
     * "MSAA quality for parent != child"). */
    [[nodiscard]] bool learn_samples(ClientTransport &transport);
    uint32_t sample_slot(UINT samples) const;
    bool     multisampled(uint32_t format) const;
    uint32_t pipeline_level() const;
    HRESULT  options(void *data, UINT bytes) const;
    HRESULT  shader(void *data, UINT bytes) const;
    HRESULT  shader_models(void *data, UINT bytes) const;
    /* Writes `bytes` of `answer` over the caller's buffer, zeroing the rest:
     * the runtime sizes each question by the revision it speaks. */
    HRESULT  fill(void *data, UINT bytes, const void *answer, UINT answer_bytes) const;

    d3d12proto::Caps12 caps_;
    uint32_t           support1_[format_count];
    uint32_t           support2_[format_count];
    uint32_t           qualities_[format_count][sample_counts];
    bool               learned_;
};

}  // namespace av::d3d12

#endif  // AV_UM_D3D12_HOST_CAPS_HPP
