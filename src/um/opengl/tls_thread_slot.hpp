#ifndef AV_UM_TLS_THREAD_SLOT_HPP
#define AV_UM_TLS_THREAD_SLOT_HPP

#include "icd/opengl/thread_slot.hpp"
#include "um/common/windows_calling_thread.hpp"

#include <stdint.h>

namespace av {

class TlsThreadSlot final : public glicd::ThreadSlot {
public:
    TlsThreadSlot();
    ~TlsThreadSlot();

    void    *get() override;
    void     set(void *value) override;
    uint32_t thread_id() override;
    void     set_last_error(uint32_t code) override;

private:
    WindowsCallingThread caller_;
    uint32_t index_;
};

}

#endif
