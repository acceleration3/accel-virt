/* While one lives, the shared textures the calling thread makes through
 * Direct3D 11 on an avgpu adapter are laid out as rows on the host, for an API
 * of ours that reads their memory as rows: a Vulkan or OpenGL window's chain
 * slot, an OpenCL image's twin (ResourceMisc, SharedRowsRequest). Asked of
 * every module the process has loaded that exports both AvAskSharedRows and
 * AvWithdrawSharedRows - the user-mode driver the texture's device runs, and,
 * with a copy per GPU vendor, any other the process holds - and withdrawn
 * from each of those; where none is loaded nothing is asked, and the texture
 * is tiled as any other. Translation only. */
#ifndef AV_UM_SHARED_ROWS_SCOPE_HPP
#define AV_UM_SHARED_ROWS_SCOPE_HPP

#include <stdint.h>

namespace av {

class SharedRowsScope {
public:
    SharedRowsScope();
    ~SharedRowsScope();
    SharedRowsScope(const SharedRowsScope &) = delete;
    SharedRowsScope &operator=(const SharedRowsScope &) = delete;

private:
    using Withdraw = void (*)();

    /* Nothing for a module without both exports, or one already asked. */
    void ask(void *module);

    /* More than a process holds: one copy per vendor in the package. */
    static constexpr uint32_t max_asked = 8u;

    /* The AvWithdrawSharedRows of each module asked. */
    Withdraw withdraw_[max_asked];
    uint32_t asked_;
};

}  // namespace av

#endif  // AV_UM_SHARED_ROWS_SCOPE_HPP
