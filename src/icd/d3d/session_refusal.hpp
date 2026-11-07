#ifndef AV_D3D_SESSION_REFUSAL_HPP
#define AV_D3D_SESSION_REFUSAL_HPP

#include "icd/d3d/hresult.hpp"

namespace av::d3d {

class SessionRefusal {
public:
    explicit SessionRefusal(bool exhausted);
    SessionRefusal(const SessionRefusal &) = delete;
    SessionRefusal &operator=(const SessionRefusal &) = delete;

    Hresult for_d3d() const;
    Hresult for_d3d9() const;

private:
    bool exhausted_;
};

}

#endif
