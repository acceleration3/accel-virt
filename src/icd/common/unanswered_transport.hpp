/* A call whose only answer is its status, sent without waiting for it: the
 * generated client asks for an answer, and gets success from here while the
 * command goes out asynchronously. For calls the ICD has already judged -
 * setting a kernel argument, an enqueue that wants no event, a release - that
 * a host refuses only on a misuse the ICD's own checks did not see. Each was a
 * round trip; a kernel launch was one per argument and one more. */
#ifndef AV_ICD_UNANSWERED_TRANSPORT_HPP
#define AV_ICD_UNANSWERED_TRANSPORT_HPP

#include "proto/client/client_transport.hpp"

#include <stdint.h>

namespace av::icd {

class UnansweredTransport final : public ClientTransport {
public:
    explicit UnansweredTransport(ClientTransport &inner);
    UnansweredTransport(const UnansweredTransport &) = delete;
    UnansweredTransport &operator=(const UnansweredTransport &) = delete;

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    /* Sent asynchronously; `reply` is a status of zero, encoded. */
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

private:
    ClientTransport &inner_;
    /* A zero status as the generated decoder reads it: one varint byte. */
    uint8_t          success_[1];
};

}  // namespace av::icd

#endif  // AV_ICD_UNANSWERED_TRANSPORT_HPP
