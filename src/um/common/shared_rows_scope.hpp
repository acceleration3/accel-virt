/* While one lives, the shared textures the calling thread makes through
 * Direct3D 11 on an avgpu adapter are laid out as rows on the host, for an API
 * of ours that reads their memory as rows: a Vulkan or OpenGL window's chain
 * slot, an OpenCL image's twin (ResourceMisc, SharedRowsRequest). Asked of
 * avumd.dll, already loaded by the device the texture is made on, through its
 * AvAskSharedRows and AvWithdrawSharedRows; where it is not loaded nothing is
 * asked, and the texture is tiled as any other. Translation only. */
#ifndef AV_UM_SHARED_ROWS_SCOPE_HPP
#define AV_UM_SHARED_ROWS_SCOPE_HPP

namespace av {

class SharedRowsScope {
public:
    SharedRowsScope();
    ~SharedRowsScope();
    SharedRowsScope(const SharedRowsScope &) = delete;
    SharedRowsScope &operator=(const SharedRowsScope &) = delete;

private:
    /* AvWithdrawSharedRows, null when nothing was asked. */
    void (*withdraw_)();
};

}  // namespace av

#endif  // AV_UM_SHARED_ROWS_SCOPE_HPP
