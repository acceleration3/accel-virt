#include "device/isolation/remote_context.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_window.hpp"
#include "device/host/host.hpp"
#include "device/isolation/renderer_link.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/renderer/bind_context_message.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/fence_page.hpp"

namespace av {

RemoteContext::RemoteContext(Host &host, BlobTable &blobs, FenceInterrupts &interrupts, uint32_t id)
    : host_(host), interrupts_(interrupts), binding_(host, blobs), id_(id), doorbell_event_(nullptr), lost_(false)
{
    AV_SCOPE("RemoteContext::RemoteContext");
}

RemoteContext::~RemoteContext()
{
    AV_SCOPE("RemoteContext::~RemoteContext");
    unbind();
}

Status RemoteContext::connect_doorbell(uint32_t index, int &descriptor)
{
    AV_SCOPE("RemoteContext::connect_doorbell");
    descriptor = -1;
    const Status made = host_.create_event(doorbell_event_);
    if (made != Status::ok) return made;
    descriptor = host_.event_descriptor(doorbell_event_);
    if (descriptor < 0) {
        host_.destroy_event(doorbell_event_);
        doorbell_event_ = nullptr;
        return Status::not_supported;
    }
    host_.connect_doorbell(doorbell_event_, index);
    return Status::ok;
}

void RemoteContext::disconnect_doorbell()
{
    AV_SCOPE("RemoteContext::disconnect_doorbell");
    if (doorbell_event_ == nullptr) return;
    host_.disconnect_doorbell(doorbell_event_, binding_.doorbell());
    host_.destroy_event(doorbell_event_);
    doorbell_event_ = nullptr;
}

Status RemoteContext::bind(const ContextSetup &setup, const CapsetInfo &capset, RendererLink &link, Status &outcome)
{
    AV_SCOPE("RemoteContext::bind");
    host_.require_main_loop("RemoteContext::bind");
    outcome = Status::invalid_state;
    if (binding_.bound() || lost_) return Status::ok;
    uint8_t setup_bytes[ContextSetup::encoded_size] = {};
    outcome = Status::invalid_parameter;
    if (setup.encode(setup_bytes, sizeof setup_bytes) != Status::ok) return Status::ok;
    outcome = binding_.bind(setup, capset);
    if (outcome != Status::ok) return Status::ok;
    ScopeExit unbind([this] { binding_.unbind(); });
    int descriptor = -1;
    outcome = connect_doorbell(setup.doorbell(), descriptor);
    if (outcome != Status::ok) return Status::ok;
    ScopeExit disconnect([this] { disconnect_doorbell(); });
    const BindContextMessage message(id_, setup_bytes, static_cast<uint32_t>(setup.size()), setup.doorbell());
    uint8_t                  body[BindContextMessage::fixed_size + ContextSetup::encoded_size] = {};
    outcome = Status::invalid_parameter;
    if (message.encode(body, sizeof body) != Status::ok) return Status::ok;
    const Status sent = link.request(RendererMessageHeader::Kind::bind_context, body,
                                     static_cast<uint32_t>(message.encoded_length()), &descriptor, 1u, outcome);
    if (sent != Status::ok || outcome != Status::ok) return sent;
    disconnect.dismiss();
    unbind.dismiss();
    return Status::ok;
}

void RemoteContext::unbind()
{
    AV_SCOPE("RemoteContext::unbind");
    disconnect_doorbell();
    binding_.unbind();
}

void RemoteContext::lose()
{
    AV_SCOPE("RemoteContext::lose");
    host_.require_main_loop("RemoteContext::lose");
    if (!binding_.bound() || lost_) return;
    lost_ = true;
    disconnect_doorbell();
    const FenceWindow window = binding_.fence_window();
    FencePage         last;
    if (window.read(last) != Status::ok) last = FencePage(0u, 0u);
    static_cast<void>(window.publish(FencePage(last.completed_fence(), last.progress() + 1u,
                                               static_cast<uint32_t>(FaultCode::device_lost))));
    interrupts_.raise(binding_.doorbell());
}

uint32_t RemoteContext::id() const
{
    AV_SCOPE("RemoteContext::id");
    return id_;
}

bool RemoteContext::bound() const
{
    AV_SCOPE("RemoteContext::bound");
    return binding_.bound();
}

bool RemoteContext::lost() const
{
    AV_SCOPE("RemoteContext::lost");
    return lost_;
}

SubApi RemoteContext::sub_api() const
{
    AV_SCOPE("RemoteContext::sub_api");
    return binding_.sub_api();
}

uint32_t RemoteContext::doorbell() const
{
    AV_SCOPE("RemoteContext::doorbell");
    return binding_.doorbell();
}

bool RemoteContext::invariants_hold() const
{
    AV_SCOPE("RemoteContext::invariants_hold");
    return doorbell_event_ == nullptr || (binding_.bound() && !lost_);
}

}
