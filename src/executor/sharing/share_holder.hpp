/* One session's hold on a Share: the backend that imported it through its
 * descriptor (feature 053), the handle its API names the import by, and
 * whatever else the backend said releasing it takes - which the backend is
 * handed back when the hold ends, so no backend keeps a table of its own. */
#ifndef AV_SHARE_HOLDER_HPP
#define AV_SHARE_HOLDER_HPP

#include <stdint.h>

namespace av {

class SessionBackend;

class ShareHolder {
public:
    ShareHolder(SessionBackend &session, uint64_t handle, uint64_t detail);

    SessionBackend &session() const;
    uint64_t        handle() const;
    uint64_t        detail() const;
    ShareHolder    *next() const;
    void            link(ShareHolder *next);

private:
    SessionBackend *session_;
    ShareHolder    *next_;
    uint64_t        handle_;
    uint64_t        detail_;
};

}  // namespace av

#endif  // AV_SHARE_HOLDER_HPP
