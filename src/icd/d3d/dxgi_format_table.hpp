/* What a Direct3D runtime is told about each DXGI format: its DDI support
 * bits and its multisample quality levels, from what the host's device
 * answered for the same format. D3D11 and D3D12 ask the same two questions in
 * the same bits, and both runtimes check the answers against each other the
 * same way (LLOCompleteLayerConstruction) - a device is refused over one
 * inconsistent family ("MSAA quality for parent != child", "MSAA quality
 * reported to be 0") - so both drivers answer from this one table.
 *
 * The host's answers are per format and need not agree within a family: a
 * GPU can render one member multisampled and not its siblings (an Intel GPU
 * renders R32G32B32_FLOAT at 2 to 16 samples, its typeless parent and its
 * UINT and SINT siblings at none), or one member at fewer counts than the
 * rest. The runtime holds a family to one story: the typeless parent answers
 * the family's levels, a member that renders multisampled answers the same,
 * every other member answers none - and every member takes part, rendering
 * multisampled or loading from a multisampled view; one that does neither
 * beside a parent that multisamples is refused. So each family settles on one
 * set of counts and the members that render at all of them, the rest only
 * loading; a family with a member that can do neither does not multisample.
 * Of every such choice the one offering the most member-and-count pairs is
 * taken, so one weak member neither refuses the driver nor takes more from
 * its siblings than it must. */
#ifndef AV_ICD_D3D_DXGI_FORMAT_TABLE_HPP
#define AV_ICD_D3D_DXGI_FORMAT_TABLE_HPP

#include "proto/d3d/dxgi_format.hpp"

#include <stdint.h>

namespace av::d3d {

class DxgiFormatTable {
public:
    /* 1, 2, 4, 8 and 16 samples. */
    static constexpr uint32_t sample_steps = 5u;

    DxgiFormatTable();
    DxgiFormatTable(const DxgiFormatTable &) = delete;
    DxgiFormatTable &operator=(const DxgiFormatTable &) = delete;

    /* The host's D3D11_FORMAT_SUPPORT and _SUPPORT2 words for `format`
     * (D3D12_FORMAT_SUPPORT1 and 2 are the same bits). */
    void record(uint32_t format, uint32_t support, uint32_t support2);
    /* The host's quality level count for `format` at `samples`. */
    void record_samples(uint32_t format, uint32_t samples, uint32_t levels);
    /* Every answer recorded: each family's counts, and the members that keep
     * multisampling, are settled. */
    void settle();

    /* D3D10_DDI_FORMAT_SUPPORT_* bits, which D3D12DDI_FORMAT_SUPPORT_* repeat,
     * and level counts: each family one story. */
    uint32_t format_support(uint32_t format) const;
    /* D3D12DDI_FORMAT_SUPPORT_TILED for a format the host tiles, 0 for one it
     * does not: a driver that carries tiled resources adds it. */
    uint32_t tiled_support(uint32_t format) const;
    uint32_t quality_levels(uint32_t format, uint32_t samples) const;
    /* Whether the host would multisample `format` at all - worth asking its
     * level counts for. */
    bool     multisample_candidate(uint32_t format) const;

private:
    /* The support bits before multisampling is judged. */
    uint32_t carried(uint32_t format) const;
    /* Claims multisampled rendering, and its family can do it at some count. */
    bool     multisamples(uint32_t format) const;
    /* Its family multisamples at some count. */
    bool     family_multisamples(uint32_t format) const;
    uint32_t step_of(uint32_t samples) const;
    /* Settles the family under `head`: its counts and who keeps them. */
    void     settle_family(uint32_t head);

    uint32_t support_[DxgiFormat::count];
    uint32_t support2_[DxgiFormat::count];
    uint32_t levels_[DxgiFormat::count][sample_steps];
    /* Per family head: the levels every member that keeps multisampling
     * answers at each count, and the typeless head with them. */
    uint32_t family_levels_[DxgiFormat::count][sample_steps];
    /* Per member: whether it keeps multisampling. */
    bool     keeps_[DxgiFormat::count];
};

}  // namespace av::d3d

#endif  // AV_ICD_D3D_DXGI_FORMAT_TABLE_HPP
