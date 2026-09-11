/* Why FakeGpuMmu refused an adapter's GPU MMU description - one value per check
 * ADAPTER_RENDER::Initialize @ 1401b3d4c (dxgkrnl.sys) and
 * VIDMM_PHYSICAL_ADAPTER::InitializeGpuVaState @ 1400972cc (dxgmms2.sys) apply. */
#ifndef AV_GPU_MMU_RULE_HPP
#define AV_GPU_MMU_RULE_HPP

namespace av {

enum class GpuMmuRule : unsigned {
    none,
    caps_query_failed,          /* "Failed to query GpuMmu caps" */
    virtual_address_bits,       /* "GpuMmu.VirtualAddressBitCount is invalid. It should be from %d to %d" */
    level_count,                /* "GpuMmu.PageDirectoryCount is invalid" */
    level_query_failed,
    level_alignment,            /* "Page table alignment of level %d must be power of 2" */
    node_metadata_failed,
    node_gpu_mmu_without_caps,  /* "Driver reported GpuMmu support for a node, but the adapter caps do not match." */
    node_reserved_flags,
    /* VidMm, surfacing from start as STATUS_NO_MEMORY */
    leaf_table_size,            /* "Page table size, specified by the driver, is invalid" */
    page_table_segment,
    paging_process_page_table_segment,
    leaf_index_bits,            /* "GpuMmu.PageTableIndexBitCount is invalid" */
    update_mode,                /* "GpuMmu.PageTableUpdateMode is invalid" */
    directory_index_bits,       /* "The number of bits in page directory levels is inconsistent ..." */
    directory_table_size,       /* "Invalid page table size" */
    total_index_bits,           /* "VirtualAddressBitCount from the caps is different from computed ..." */
};

}  // namespace av

#endif  // AV_GPU_MMU_RULE_HPP
