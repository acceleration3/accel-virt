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

    [[nodiscard]] Status assign(const DxgkargSetRootPageTableRecord &root);
    bool                 admits(uint64_t address) const;

    bool invariants_hold() const;

private:
    Platform             &platform_;
    const GpuMmuGeometry &geometry_;
    uint32_t              entries_;

    friend struct RootPageTableProbe;
};

}

#endif
