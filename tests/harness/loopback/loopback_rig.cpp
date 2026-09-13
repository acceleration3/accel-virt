#include "harness/loopback/loopback_rig.hpp"

#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/submission_descriptor.hpp"

namespace av {

namespace {

constexpr std::uint32_t resources_per_context = 10u;

}  // namespace

LoopbackRig::LoopbackRig(const std::uint8_t *gpu)
    : platform_(), device_(platform_, gpu), driver_(platform_, device_), query_(platform_),
      first_(platform_, device_.doorbells()), second_(platform_, device_.doorbells()) {}

LoopbackRig::~LoopbackRig() { device_.device().reset(); }

MockPlatform   &LoopbackRig::platform() { return platform_; }
LoopbackDevice &LoopbackRig::device() { return device_; }
LoopbackDriver &LoopbackRig::driver() { return driver_; }
GuestContext   &LoopbackRig::context(std::uint32_t index) { return index == 0u ? first_ : second_; }
std::uint32_t   LoopbackRig::line(std::uint32_t index) const { return index + 1u; }

const CapsetInfo &LoopbackRig::capset() const { return query_.capset(); }

bool LoopbackRig::read_capset() { return driver_.query(query_) == Status::ok && query_.complete(); }

Status LoopbackRig::set_up(std::uint32_t index)
{
    const std::uint32_t base = (index + 1u) * resources_per_context;
    const ContextSetup  setup(ProtocolVersion(1u, 0u), SubApi::vulkan, base, ring_order, base + 1u, line(index),
                              base + 2u);
    return context(index).set_up(query_.capset(), index + 1u, setup);
}

bool LoopbackRig::establish(std::uint32_t index)
{
    return set_up(index) == Status::ok && driver_.converse(context(index)) == 5u &&
           context(index).state() == GuestContext::State::established;
}

bool LoopbackRig::retire(std::uint32_t index)
{
    platform_.set_irql(Irql::dispatch);
    const Status torn = context(index).tear_down();
    platform_.set_irql(Irql::passive);
    static_cast<void>(driver_.converse(context(index)));
    return torn == Status::ok && context(index).release() == Status::ok &&
           context(index).state() == GuestContext::State::idle;
}

Status LoopbackRig::submit(std::uint32_t index, std::uint64_t fence, std::uint64_t begin, std::uint64_t end)
{
    platform_.set_irql(Irql::dispatch);
    const Status status = context(index).submit(SubmissionDescriptor(fence, begin, end));
    platform_.set_irql(Irql::passive);
    return status;
}

Status LoopbackRig::serve(std::uint32_t index)
{
    platform_.set_irql(Irql::dispatch);
    const Status status = context(index).serve();
    platform_.set_irql(Irql::passive);
    return status;
}

}  // namespace av
