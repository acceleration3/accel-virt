#ifndef AV_UM_WINDOWS_CALLING_THREAD_HPP
#define AV_UM_WINDOWS_CALLING_THREAD_HPP

#include "icd/common/calling_thread.hpp"

namespace av {

class WindowsCallingThread final : public icd::CallingThread {
public:
    uint32_t thread_id() override;
};

}

#endif
