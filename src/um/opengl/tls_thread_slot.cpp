#include "um/opengl/tls_thread_slot.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

TlsThreadSlot::TlsThreadSlot() : caller_(), index_(TlsAlloc())
{
    AV_SCOPE("TlsThreadSlot::TlsThreadSlot");
}

TlsThreadSlot::~TlsThreadSlot()
{
    AV_SCOPE("TlsThreadSlot::~TlsThreadSlot");
    if (index_ != TLS_OUT_OF_INDEXES) static_cast<void>(TlsFree(index_));
}

/* With no index every thread has nothing current, and a GL call does nothing. */
void *TlsThreadSlot::get()
{
    AV_SCOPE("TlsThreadSlot::get");
    return index_ != TLS_OUT_OF_INDEXES ? TlsGetValue(index_) : nullptr;
}

void TlsThreadSlot::set(void *value)
{
    AV_SCOPE("TlsThreadSlot::set");
    if (index_ != TLS_OUT_OF_INDEXES) static_cast<void>(TlsSetValue(index_, value));
}

uint32_t TlsThreadSlot::thread_id()
{
    AV_SCOPE("TlsThreadSlot::thread_id");
    return caller_.thread_id();
}

void TlsThreadSlot::set_last_error(uint32_t code)
{
    AV_SCOPE("TlsThreadSlot::set_last_error");
    SetLastError(code);
}

}  // namespace av
