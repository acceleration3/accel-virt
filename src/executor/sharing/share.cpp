#include "executor/sharing/share.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/sharing/share_holder.hpp"

namespace av {

Share::Share(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, ShareOrigin origin)
    : id_(id), size_(size), direct3d_(origin), holders_(nullptr), publisher_(nullptr), domain_(domain), kind_(kind),
      handle_kind_(0u), pins_(0u), descriptor_(), origin_(origin), guest_holds_(true), indexed_(true)
{
    AV_SCOPE("Share::Share");
}

uint32_t Share::domain() const
{
    AV_SCOPE("Share::domain");
    return domain_;
}

uint64_t Share::id() const
{
    AV_SCOPE("Share::id");
    return id_;
}

uint32_t Share::kind() const
{
    AV_SCOPE("Share::kind");
    return kind_;
}

uint64_t Share::size() const
{
    AV_SCOPE("Share::size");
    return size_;
}

ShareOrigin Share::origin() const
{
    AV_SCOPE("Share::origin");
    return origin_;
}

bool Share::accepts(uint32_t kind, uint64_t size) const
{
    AV_SCOPE("Share::accepts");
    return kind == kind_ && size == size_;
}

void Share::describe(uint32_t kind, uint64_t size)
{
    AV_SCOPE("Share::describe");
    kind_ = kind;
    size_ = size;
}

SessionBackend *Share::publisher() const
{
    AV_SCOPE("Share::publisher");
    return publisher_;
}

void Share::publish_by(SessionBackend *session)
{
    AV_SCOPE("Share::publish_by");
    publisher_ = session;
}

bool Share::indexed() const
{
    AV_SCOPE("Share::indexed");
    return indexed_;
}

void Share::unindex()
{
    AV_SCOPE("Share::unindex");
    indexed_ = false;
}

bool Share::guest_holds() const
{
    AV_SCOPE("Share::guest_holds");
    return guest_holds_;
}

void Share::let_go_by_guest()
{
    AV_SCOPE("Share::let_go_by_guest");
    guest_holds_ = false;
}

bool Share::unheld() const
{
    AV_SCOPE("Share::unheld");
    return !guest_holds_ && holders_ == nullptr && pins_ == 0u;
}

void Share::pin()
{
    AV_SCOPE("Share::pin");
    pins_++;
}

void Share::unpin()
{
    AV_SCOPE("Share::unpin");
    if (pins_ != 0u) pins_--;
}

ShareHolder *Share::holders() const
{
    AV_SCOPE("Share::holders");
    return holders_;
}

ShareHolder *Share::holder_of(const SessionBackend &session) const
{
    AV_SCOPE("Share::holder_of");
    for (ShareHolder *holder = holders_; holder != nullptr; holder = holder->next())
        if (&holder->session() == &session) return holder;
    return nullptr;
}

ShareHolder *Share::holder_into(const SessionBackend &session, uint64_t into) const
{
    AV_SCOPE("Share::holder_into");
    for (ShareHolder *holder = holders_; holder != nullptr; holder = holder->next())
        if (&holder->session() == &session && holder->into() == into) return holder;
    return nullptr;
}

ShareHolder *Share::holder_answered(const SessionBackend &session, uint64_t handle) const
{
    AV_SCOPE("Share::holder_answered");
    for (ShareHolder *holder = holders_; holder != nullptr; holder = holder->next())
        if (&holder->session() == &session && holder->handle() == handle) return holder;
    return nullptr;
}

void Share::hold(ShareHolder &holder)
{
    AV_SCOPE("Share::hold");
    holder.link(holders_);
    holders_ = &holder;
}

void Share::unhold(ShareHolder &holder)
{
    AV_SCOPE("Share::unhold");
    if (holders_ == &holder) {
        holders_ = holder.next();
        holder.link(nullptr);
        return;
    }
    for (ShareHolder *walk = holders_; walk != nullptr; walk = walk->next()) {
        if (walk->next() != &holder) continue;
        walk->link(holder.next());
        holder.link(nullptr);
        return;
    }
}

int Share::descriptor() const
{
    AV_SCOPE("Share::descriptor");
    return descriptor_.fd();
}

uint32_t Share::handle_kind() const
{
    AV_SCOPE("Share::handle_kind");
    return handle_kind_;
}

void Share::keep_descriptor(uint32_t handle_kind, OwnedFd &descriptor)
{
    AV_SCOPE("Share::keep_descriptor");
    handle_kind_ = handle_kind;
    descriptor_.hold(descriptor.take());
}

void Share::drop_descriptor()
{
    AV_SCOPE("Share::drop_descriptor");
    descriptor_.close();
}

D3dShareState &Share::direct3d()
{
    AV_SCOPE("Share::direct3d");
    return direct3d_;
}

const D3dShareState &Share::direct3d() const
{
    AV_SCOPE("Share::direct3d");
    return direct3d_;
}

const SharedImageShape &Share::shape() const
{
    AV_SCOPE("Share::shape");
    return shape_;
}

void Share::describe_image(const SharedImageShape &shape)
{
    AV_SCOPE("Share::describe_image");
    shape_ = shape;
}

}
