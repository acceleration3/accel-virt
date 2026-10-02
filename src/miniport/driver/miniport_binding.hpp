/* The one pointer DxgkDdiAddDevice and DxgkDdiUnload use to reach the driver's
 * root: those DDIs carry no context, so this is the only process-lifetime state
 * core has (constitution III). DriverEntry binds it; the harness binds its own. */
#ifndef AV_MINIPORT_BINDING_HPP
#define AV_MINIPORT_BINDING_HPP

#include <stdint.h>

namespace av {

class ImageExit;
class Miniport;

class MiniportBinding {
public:
    /* `exit`, where there is one, is what unload ends with. */
    void      bind(Miniport &miniport, ImageExit *exit = nullptr);
    void      unbind();
    Miniport *miniport() const;

    /* The context-less DDIs, answered with an NTSTATUS; with nothing bound they
     * fail instead of dereferencing. */
    int32_t add_device(void *physical_device_object, void **context);
    int32_t remove_device(void *context);
    void    unload();

private:
    Miniport  *miniport_ = nullptr;
    ImageExit *exit_ = nullptr;
};

}  // namespace av

#endif  // AV_MINIPORT_BINDING_HPP
