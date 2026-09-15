#include "icd/vulkan/loader_handle.hpp"

namespace av::icd {

LoaderHandle::LoaderHandle(Kind kind, uint64_t host, Connection *connection, LoaderHandle *parent)
    : loader_data_(reinterpret_cast<void *>(loader_magic)), host_(host), connection_(connection), kind_(kind),
      parent_(parent) {}

LoaderHandle::Kind LoaderHandle::kind() const { return kind_; }
uint64_t           LoaderHandle::host() const { return host_; }
Connection        *LoaderHandle::connection() const { return connection_; }
LoaderHandle      *LoaderHandle::parent() const { return parent_; }

bool LoaderHandle::names(Kind kind, uint64_t host, const Connection *connection) const
{
    return kind_ == kind && host_ == host && connection_ == connection;
}

bool LoaderHandle::descends_from(const LoaderHandle &ancestor) const
{
    for (const LoaderHandle *above = parent_; above != nullptr; above = above->parent_)
        if (above == &ancestor) return true;
    return false;
}

bool LoaderHandle::invariants_hold() const
{
    if (host_ == 0u || connection_ == nullptr) return false;
    if (kind_ == Kind::instance) return parent_ == nullptr;
    return parent_ != nullptr && parent_->kind_ < kind_ && parent_->connection_ == connection_;
}

}  // namespace av::icd
