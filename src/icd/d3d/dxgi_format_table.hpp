/* What a Direct3D runtime is told about each DXGI format: its DDI support
 * bits and its multisample quality levels, from what the host's device
 * answered for the same format. D3D11 and D3D12 ask the same two questions in
 * the same bits, and both runtimes check the answers against each other the
 * same way (LLOCompleteLayerConstruction) - a device is refused over one
 * inconsistent family ("MSAA quality for parent != child", "MSAA quality
 * reported to be 0") - so both drivers answer from this one table. */
#ifndef AV_ICD_D3D_DXGI_FORMAT_TABLE_HPP
#define AV_ICD_D3D_DXGI_FORMAT_TABLE_HPP

#include <stdint.h>

namespace av::d3d {

class DxgiFormatTable {
public:
    static constexpr uint32_t format_count = 192u;
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
    /* Every answer recorded: each family's shared level counts are settled. */
    void settle();

    /* D3D10_DDI_FORMAT_SUPPORT_* bits, which D3D12DDI_FORMAT_SUPPORT_* repeat,
     * and level counts, as Direct3D 12 wants them: each family one story, its
     * members' multisampling pooled (D3D12Core refuses "MSAA quality for
     * parent != child"). */
    uint32_t format_support(uint32_t format) const;
    uint32_t quality_levels(uint32_t format, uint32_t samples) const;
    /* The same as Direct3D 11 wants them: each format its own answer, a
     * typeless parent included. Pooled, some formats DWM draws its acrylic
     * backdrop in lost their multisampling, and the Start menu's backdrop
     * drew black. */
    uint32_t own_format_support(uint32_t format) const;
    uint32_t own_quality_levels(uint32_t format, uint32_t samples) const;
    /* Whether the host would multisample `format` at all - worth asking its
     * level counts for. */
    bool     multisample_candidate(uint32_t format) const;

private:
    /* The family's typeless head: DXGI numbers a family's formats after it,
     * except the two BGRA families, named member by member. */
    uint32_t family_of(uint32_t format) const;
    bool     typeless(uint32_t format) const;
    bool     depth(uint32_t format) const;
    /* The support bits before multisampling is judged. */
    uint32_t carried(uint32_t format) const;
    /* Claims multisampled rendering, and its family can do it at some count. */
    bool     multisamples(uint32_t format) const;
    uint32_t step_of(uint32_t samples) const;

    uint32_t support_[format_count];
    uint32_t support2_[format_count];
    uint32_t levels_[format_count][sample_steps];
    /* Per family head: the fewest levels any member that claims multisampled
     * rendering has at each count. */
    uint32_t family_levels_[format_count][sample_steps];
};

}  // namespace av::d3d

#endif  // AV_ICD_D3D_DXGI_FORMAT_TABLE_HPP
