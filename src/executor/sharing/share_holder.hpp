#ifndef AV_SHARE_HOLDER_HPP
#define AV_SHARE_HOLDER_HPP

#include <stdint.h>

namespace av {

class SessionBackend;

class ShareHolder {
public:
    ShareHolder(SessionBackend &session, uint64_t into, uint64_t handle, uint64_t detail);

    SessionBackend &session() const;
    uint64_t        into() const;
    uint64_t        handle() const;
    uint64_t        detail() const;
    ShareHolder    *next() const;
    void            link(ShareHolder *next);

private:
    SessionBackend *session_;
    ShareHolder    *next_;
    uint64_t        into_;
    uint64_t        handle_;
    uint64_t        detail_;
};

}

#endif
