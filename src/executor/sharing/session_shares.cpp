#include "executor/sharing/session_shares.hpp"

#include "executor/sharing/shared_image_shape.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/sharing/d3d_shares.hpp"
#include "executor/sharing/host_shares.hpp"

namespace av {

SessionShares::SessionShares() : session_(nullptr), shares_(nullptr), domain_(0u)
{
    AV_SCOPE("SessionShares::SessionShares");
}

void SessionShares::bind(HostShares &shares, uint32_t domain, SessionBackend &session)
{
    AV_SCOPE("SessionShares::bind");
    shares_ = &shares;
    domain_ = domain;
    session_ = &session;
}

bool SessionShares::bound() const
{
    AV_SCOPE("SessionShares::bound");
    return shares_ != nullptr;
}

uint32_t SessionShares::domain() const
{
    AV_SCOPE("SessionShares::domain");
    return domain_;
}

HostShares *SessionShares::shares() const
{
    AV_SCOPE("SessionShares::shares");
    return shares_;
}

Status SessionShares::publish(uint64_t id, uint32_t kind, uint64_t size, uint64_t handle)
{
    AV_SCOPE("SessionShares::publish");
    if (shares_ == nullptr) return Status::invalid_state;
    return shares_->publish(domain_, id, kind, size, *session_, handle);
}

Status SessionShares::open(uint64_t id, uint32_t kind, uint64_t size, uint64_t into, uint64_t &handle)
{
    AV_SCOPE("SessionShares::open");
    handle = 0u;
    if (shares_ == nullptr) return Status::invalid_state;
    return shares_->open(domain_, id, kind, size, *session_, into, handle);
}

void SessionShares::close(uint64_t id, uint64_t handle)
{
    AV_SCOPE("SessionShares::close");
    if (shares_ != nullptr) shares_->close(domain_, id, *session_, handle);
}

Status SessionShares::publish_resource(uint64_t id, IUnknown *resource, ShareOrigin origin)
{
    AV_SCOPE("SessionShares::publish_resource");
    if (shares_ == nullptr) return Status::invalid_state;
    return D3dShares(*shares_).publish_resource(domain_, id, resource, origin, *session_);
}

void SessionShares::written(uint64_t id)
{
    AV_SCOPE("SessionShares::written");
    if (shares_ != nullptr) D3dShares(*shares_).written(domain_, id);
}

bool SessionShares::describe_image(uint64_t id, const SharedImageShape &shape)
{
    AV_SCOPE("SessionShares::describe_image");
    return shares_ != nullptr && shares_->describe_image(domain_, id, shape);
}

void SessionShares::end()
{
    AV_SCOPE("SessionShares::end");
    if (shares_ != nullptr) shares_->session_closed(*session_);
    shares_ = nullptr;
    session_ = nullptr;
    domain_ = 0u;
}

}
