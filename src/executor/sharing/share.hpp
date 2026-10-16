/* One guest shared object on one host GPU: the id the guest's driver minted
 * over its allocation, in the domain (guest device) that minted it, and all
 * the host knows of it - whatever API made it, the descriptor other APIs
 * import it through, the Direct3D resources DWM and the sessions open, and
 * who holds it.
 *
 * The guest holds every share from its publish until its driver says the
 * object is gone from the whole guest (release). A session that imported it
 * holds it until it closes it or ends. The publisher does not: what it made
 * is its own, and a Direct3D session that opens a resource holds it by its
 * own COM reference, so a share that held either would keep a texture the
 * guest let go of until the application exited. A share no one holds, and no
 * caller is using (pinned), is freed by HostShares. The COM references and
 * the descriptor here are the share's own. */
#ifndef AV_SHARE_HPP
#define AV_SHARE_HPP

#include "executor/owned_fd.hpp"
#include "executor/sharing/shared_image_shape.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class SessionBackend;
class ShareHolder;

/* Which way a share came to the host. */
enum class ShareOrigin : uint8_t {
    /* A session's own object, published by its handle (feature 053). */
    object,
    /* A Direct3D 11 resource, published as it was created. */
    d3d11,
    /* A Direct3D 12 resource, shown through a Direct3D 11 view. */
    d3d12
};

class Share {
public:
    Share(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, ShareOrigin origin);

    uint32_t    domain() const;
    uint64_t    id() const;
    uint32_t    kind() const;
    uint64_t    size() const;
    ShareOrigin origin() const;
    /* Whether an open naming `kind` and `size` names this object: a guest
     * that could open memory as a fence, or past its end, could have the host
     * read what is not there. */
    bool        accepts(uint32_t kind, uint64_t size) const;
    /* What a Direct3D share is to other APIs, said once it is published by
     * handle too. */
    void        describe(uint32_t kind, uint64_t size);

    /* The session that published it, which may publish it again and may not
     * open it; null once that session ended. Not a hold. */
    SessionBackend *publisher() const;
    void            publish_by(SessionBackend *session);

    /* Whether the index names it. A device reset unindexes what sessions
     * still hold, as the guest mints the same ids again. */
    bool indexed() const;
    void unindex();

    bool guest_holds() const;
    void let_go_by_guest();
    /* Held by no one, the guest included, and used by no caller. */
    bool unheld() const;
    /* A caller is using it outside HostShares' lock: it is not freed until
     * the caller lets it go. */
    void pin();
    void unpin();

    ShareHolder *holders() const;
    /* The hold `session` has, or null. */
    ShareHolder *holder_of(const SessionBackend &session) const;
    void         hold(ShareHolder &holder);
    void         unhold(ShareHolder &holder);

    /* The descriptor other APIs import it through, and which AV_SHARED_HANDLE_*
     * kind it is; -1 when none was made. */
    int      descriptor() const;
    uint32_t handle_kind() const;
    /* `descriptor` kept as the share's from now on. */
    void     keep_descriptor(uint32_t handle_kind, OwnedFd &descriptor);
    /* The descriptor closed: nothing imports through it any more. */
    void     drop_descriptor();

    /* The resource as it was published, with the share's reference. */
    IUnknown *resource() const;
    void      keep_resource(IUnknown *resource);
    /* What a Direct3D 11 session opens: the view made over a Direct3D 12
     * resource, or the resource itself. */
    IUnknown *view() const;
    void      keep_view(IUnknown *view);
    /* A flip presentation's copy, which a compositor opens in its place. */
    IUnknown *shown() const;
    void      keep_shown(IUnknown *shown);
    /* Every COM object the share holds, handed over with its reference and
     * forgotten here: at most three. */
    uint32_t  give_up_objects(IUnknown **objects);
    /* What is a lost Direct3D device's, handed over and forgotten likewise:
     * of Direct3D 11's, the view and the flip copy - both always its - and
     * the resource of a share it published; of Direct3D 12's, the resource
     * of a share it published alone - the view made over it is Direct3D
     * 11's, over memory the lost device's resource only shared, and goes on
     * opening as the surface it was. At most three. */
    uint32_t  give_up_direct3d11(IUnknown **objects);
    uint32_t  give_up_direct3d12(IUnknown **objects);

    /* What its memory is as an image, as the publisher described it; empty
     * for memory that is none. */
    const SharedImageShape &shape() const;
    void                    describe_image(const SharedImageShape &shape);

    void     written();
    uint64_t writes() const;
    /* One more Direct3D open of it, for the report. */
    void     opened();
    uint64_t opens() const;

private:
    uint64_t     id_;
    uint64_t     size_;
    uint64_t     writes_;
    uint64_t     opens_;
    IUnknown    *resource_;
    IUnknown    *view_;
    IUnknown    *shown_;
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

}  // namespace av

#endif  // AV_SHARE_HPP
