#ifndef AV_CLICD_CL_DISPATCH_TABLE_HPP
#define AV_CLICD_CL_DISPATCH_TABLE_HPP

#include <CL/cl.h>
#include <CL/cl_icd.h>

#include <stdint.h>

namespace av::clicd {

struct ClDispatchTableProbe;

class ClDispatchTable {
public:
    ClDispatchTable();
    ClDispatchTable(const ClDispatchTable &) = delete;
    ClDispatchTable &operator=(const ClDispatchTable &) = delete;

    void fill();
    bool filled() const;

    const cl_icd_dispatch &table() const;

    void *icd_get_platform_ids() const;

    void *external_mem_acquire() const;
    void *external_mem_release() const;
    void *gl_context_info() const;
    void *program_from_il() const;

    uint32_t claimed() const;

    bool invariants_hold() const;

private:
    friend struct ClDispatchTableProbe;

    cl_icd_dispatch table_;
    uint32_t        claimed_;
    bool            filled_;
};

}

#endif
