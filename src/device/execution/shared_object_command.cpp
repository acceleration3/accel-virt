#include "device/execution/shared_object_command.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/context_executor.hpp"
#include "proto/submission/shared_object_answer.hpp"
#include "proto/submission/shared_object_request.hpp"

namespace av {

SharedObjectCommand::SharedObjectCommand(ContextExecutor &executor, const CommandHeader &header,
                                         const uint8_t *body)
    : executor_(executor), header_(header), body_(body)
{
    AV_SCOPE("SharedObjectCommand::SharedObjectCommand");
}

size_t SharedObjectCommand::body_size() const
{
    AV_SCOPE("SharedObjectCommand::body_size");
    return header_.size() - CommandHeader::encoded_size;
}

uint32_t SharedObjectCommand::refusal_of(Status answered) const
{
    AV_SCOPE("SharedObjectCommand::refusal_of");
    switch (answered) {
    case Status::ok: return SharedObjectAnswer::done;
    case Status::not_found: return SharedObjectAnswer::unknown_object;
    case Status::invalid_parameter: return SharedObjectAnswer::mismatched_object;
    case Status::conflict: return SharedObjectAnswer::conflict;
    default: return SharedObjectAnswer::executor_refused;
    }
}

Status SharedObjectCommand::answer(uint32_t status, uint64_t wire_handle, uint8_t *reply, size_t capacity,
                                   size_t &produced)
{
    AV_SCOPE("SharedObjectCommand::answer");
    produced = SharedObjectAnswer::encoded_size;
    static_cast<void>(SharedObjectAnswer(status, wire_handle).encode(reply, capacity));
    return Status::ok;
}

Status SharedObjectCommand::run(uint8_t *reply, size_t capacity, size_t &produced)
{
    AV_SCOPE("SharedObjectCommand::run");
    produced = 0u;
    if (header_.id() != CommandHeader::share_object_id &&
        header_.id() != CommandHeader::open_shared_object_id &&
        header_.id() != CommandHeader::release_shared_object_id)
        return Status::not_found;
    if (body_size() != SharedObjectRequest::encoded_size) return Status::malformed;

    SharedObjectRequest request;
    if (request.decode(body_, body_size()) != Status::ok)
        return answer(SharedObjectAnswer::refused_request, 0u, reply, capacity, produced);

    if (header_.id() == CommandHeader::release_shared_object_id) {
        executor_.close_object(request.object_id(), request.wire_handle());
        return answer(SharedObjectAnswer::done, 0u, reply, capacity, produced);
    }
    if (header_.id() == CommandHeader::share_object_id) {
        if (request.wire_handle() == 0u)
            return answer(SharedObjectAnswer::refused_request, 0u, reply, capacity, produced);
        const Status published = executor_.share_object(request.object_id(), request.kind().value(), request.size(),
                                                        request.wire_handle());
        return answer(refusal_of(published), 0u, reply, capacity, produced);
    }
    uint64_t     wire_handle = 0u;
    const Status opened = executor_.open_object(request.object_id(), request.kind().value(), request.size(),
                                                request.wire_handle(), wire_handle);
    return answer(refusal_of(opened), wire_handle, reply, capacity, produced);
}

}
