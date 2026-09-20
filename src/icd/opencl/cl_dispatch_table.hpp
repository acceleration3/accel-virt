/* The one cl_icd_dispatch this ICD presents to the Khronos loader.
 *
 * The ICD ABI is positional and unchecked: the loader reads a slot and jumps
 * through it, so every slot has to hold something. This fills the ones this
 * ICD implements with its own entries and every other one with the generated
 * refusal, which answers what the API means by "this driver does not carry it"
 * and says so once into the log.
 *
 * The entries themselves are in cl_hooks.cpp, where fill() is defined beside
 * them: they are bare function pointers the loader calls with OpenCL's own
 * arguments and nowhere to put a `this`, so each is one line through
 * ClIcdBinding (constitution III). */
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

    /* This ICD's entries, then the refusals. Filling a second time changes
     * nothing: every slot is already claimed. */
    void fill();
    bool filled() const;

    const cl_icd_dispatch &table() const;

    /* The one entry the loader asks for by name rather than by slot, as the
     * address clGetExtensionFunctionAddress answers. */
    void *icd_get_platform_ids() const;

    /* How many slots this ICD claims itself, which is what a test counts to
     * know the table did not quietly shrink. */
    uint32_t claimed() const;

    bool invariants_hold() const;

private:
    friend struct ClDispatchTableProbe;

    cl_icd_dispatch table_;
    uint32_t        claimed_;
    bool            filled_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_DISPATCH_TABLE_HPP
