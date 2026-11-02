#ifndef AV_CONTEXT_HPP
#define AV_CONTEXT_HPP

#include "miniport/memory/root_page_table.hpp"
#include "proto/escape/av_context_record.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Device;
class GpuMmuGeometry;
struct DxgkargCreateContextRecord;
struct DxgkargSetRootPageTableRecord;

class Context {
public:
    static constexpr uint32_t dma_buffer_bytes = 0x10000u;
    static constexpr uint32_t allocation_list_entries = 256u;
    static constexpr uint32_t patch_location_entries = 1024u;

    Context(Platform &platform, Device &device, const GpuMmuGeometry &geometry, uint32_t node, uint32_t flags);
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;

    Device  &device() const;
    uint32_t node() const;
    uint32_t flags() const;
    void     report_to(DxgkargCreateContextRecord &request);
    [[nodiscard]] Status assign_root(const DxgkargSetRootPageTableRecord &root);
    void     become_av(const AvContextRecord &record);
    bool     is_av() const;
    const AvContextRecord &av_record() const;
    bool     admits(uint64_t dma_address) const;

private:
    Platform &platform_;
    Device   &device_;
    uint32_t  node_;
    uint32_t  flags_;
    RootPageTable root_;
    AvContextRecord av_record_;
    bool      is_av_;
};

}

#endif
