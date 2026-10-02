#include "icd/d3d/umd_connection.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::d3d {

UmdConnection::UmdConnection(Kmt &kmt) : search_(kmt), luid_(0u), opens_(0u)
{
    AV_SCOPE("UmdConnection::UmdConnection");
}

UmdConnection::~UmdConnection()
{
    AV_SCOPE("UmdConnection::~UmdConnection");
}

Status UmdConnection::acquire(uint64_t luid)
{
    AV_SCOPE("UmdConnection::acquire");
    if (search_.found()) {
        if (luid != luid_) return Status::conflict;
        opens_++;
        return Status::ok;
    }
    /* Made here rather than at namespace scope: avumd.dll has no C runtime to
     * run a static constructor, so a global one would stay zeroed in the guest
     * and match no adapter - while every Linux test, whose runtime does run
     * it, passed. */
    const Status found = search_.find_avgpu(luid);
    if (found != Status::ok) return found == Status::not_found ? Status::not_supported : found;
    luid_ = luid;
    opens_ = 1u;
    return Status::ok;
}

Status UmdConnection::release()
{
    AV_SCOPE("UmdConnection::release");
    if (opens_ == 0u) return Status::invalid_state;
    opens_--;
    if (opens_ != 0u) return Status::ok;
    return search_.close();
}

bool UmdConnection::open() const
{
    AV_SCOPE("UmdConnection::open");
    return search_.found();
}

bool UmdConnection::holds(uint64_t luid) const
{
    AV_SCOPE("UmdConnection::holds");
    return search_.found() && luid_ == luid;
}

bool UmdConnection::shares_adapter(const UmdConnection &other) const
{
    AV_SCOPE("UmdConnection::shares_adapter");
    return other.search_.found() && holds(other.luid_);
}

uint32_t UmdConnection::adapter() const
{
    AV_SCOPE("UmdConnection::adapter");
    return search_.adapter();
}

uint32_t UmdConnection::opens() const
{
    AV_SCOPE("UmdConnection::opens");
    return opens_;
}

void UmdConnection::report(StateWriter &writer) const
{
    AV_SCOPE("UmdConnection::report");
    writer.flag("open", search_.found());
    writer.number("adapter", search_.adapter());
    writer.hex("luid", luid_);
    writer.number("opens", opens_);
}

bool UmdConnection::invariants_hold() const
{
    AV_SCOPE("UmdConnection::invariants_hold");
    return search_.found() == (opens_ != 0u);
}

}  // namespace av::d3d
