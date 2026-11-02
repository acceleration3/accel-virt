#ifndef AV_GPU_MMU_RULE_HPP
#define AV_GPU_MMU_RULE_HPP

namespace av {

enum class GpuMmuRule : unsigned {
    none,
    caps_query_failed,
    virtual_address_bits,
    level_count,
    level_query_failed,
    level_alignment,
    node_metadata_failed,
    node_gpu_mmu_without_caps,
    node_reserved_flags,
    leaf_table_size,
    page_table_segment,
    paging_process_page_table_segment,
    leaf_index_bits,
    update_mode,
    directory_index_bits,
    directory_table_size,
    total_index_bits,
};

}

#endif
