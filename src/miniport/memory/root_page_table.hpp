/* The root of the page tables one context runs on, as DxgkDdiSetRootPageTable
 * names it: where VidMm put the root table and how many of its entries are in
 * use. The device walks a submission's DMA buffer from it. */
#ifndef AV_ROOT_PAGE_TABLE_HPP
#define AV_ROOT_PAGE_TABLE_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class GpuMmuGeometry;
struct RootPageTableProbe;
struct DxgkargSetRootPageTableRecord;

class RootPageTable {
public:
    RootPageTable(Platform &platform, const GpuMmuGeometry &geometry);
    RootPageTable(const RootPageTable &) = delete;
    RootPageTable &operator=(const RootPageTable &) = delete;

    /* invalid_parameter, keeping the root it had, for a root with no entries or
     * more than one table holds. */
    [[nodiscard]] Status assign(const DxgkargSetRootPageTableRecord &root);
    /* Whether a DMA buffer at `address` can be walked from this root. Any
     * address before a root is assigned: VidSchiEnsureRootPageTableUpdated
     * skips DxgkDdiSetRootPageTable for some contexts. */
    bool                 admits(uint64_t address) const;

    bool invariants_hold() const;

private:
    Platform             &platform_;
    const GpuMmuGeometry &geometry_;
    uint32_t              segment_id_;
    uint64_t              segment_offset_;
    uint32_t              entries_;

    friend struct RootPageTableProbe;
};

}  // namespace av

#endif  // AV_ROOT_PAGE_TABLE_HPP
