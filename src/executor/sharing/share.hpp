#ifndef AV_SHARE_HPP
#define AV_SHARE_HPP

#include "executor/owned_fd.hpp"
#include "executor/sharing/d3d_share_state.hpp"
#include "executor/sharing/share_origin.hpp"
#include "executor/sharing/shared_image_shape.hpp"

#include <stdint.h>

namespace av {

class SessionBackend;
class ShareHolder;

class Share {
public:
    Share(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, ShareOrigin origin);

    uint32_t    domain() const;
    uint64_t    id() const;
    uint32_t    kind() const;
    uint64_t    size() const;
    ShareOrigin origin() const;
    bool        accepts(uint32_t kind, uint64_t size) const;
    void        describe(uint32_t kind, uint64_t size);

    SessionBackend *publisher() const;
    void            publish_by(SessionBackend *session);

    bool indexed() const;
    void unindex();

    bool guest_holds() const;
    void let_go_by_guest();
    bool unheld() const;
    void pin();
    void unpin();

    ShareHolder *holders() const;
    ShareHolder *holder_of(const SessionBackend &session) const;
    ShareHolder *holder_into(const SessionBackend &session, uint64_t into) const;
    ShareHolder *holder_answered(const SessionBackend &session, uint64_t handle) const;
    void         hold(ShareHolder &holder);
    void         unhold(ShareHolder &holder);

    int      descriptor() const;
    uint32_t handle_kind() const;
    void     keep_descriptor(uint32_t handle_kind, OwnedFd &descriptor);
    void     drop_descriptor();

    D3dShareState       &direct3d();
    const D3dShareState &direct3d() const;

    const SharedImageShape &shape() const;
    void                    describe_image(const SharedImageShape &shape);

private:
    uint64_t     id_;
    uint64_t     size_;
    D3dShareState direct3d_;
    SharedImageShape shape_;
    ShareHolder *holders_;
    SessionBackend *publisher_;
    uint32_t     domain_;
    uint32_t     kind_;
    uint32_t     handle_kind_;
    uint32_t     pins_;
    OwnedFd      descriptor_;
    ShareOrigin  origin_;
    bool         guest_holds_;
    bool         indexed_;
};

}

#endif
