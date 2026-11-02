#ifndef AV_MINIPORT_BINDING_HPP
#define AV_MINIPORT_BINDING_HPP

#include <stdint.h>

namespace av {

class ImageExit;
class Miniport;

class MiniportBinding {
public:
    void      bind(Miniport &miniport, ImageExit *exit = nullptr);
    void      unbind();
    Miniport *miniport() const;

    int32_t add_device(void *physical_device_object, void **context);
    int32_t remove_device(void *context);
    void    unload();

private:
    Miniport  *miniport_ = nullptr;
    ImageExit *exit_ = nullptr;
};

}

#endif
