/* The far end of a device's export socket: a process outside QEMU as
 * docs/external-export.md describes it, holding its end of a SOCK_SEQPACKET
 * socketpair whose other end the test hands the device as the glue would an
 * accepted connection. It writes requests as a consumer does and reads
 * answers with the decoder a consumer would hold them to, keeping the one
 * descriptor that may come beside each. */
#ifndef AV_FAKE_EXPORT_CONSUMER_HPP
#define AV_FAKE_EXPORT_CONSUMER_HPP

#include "core/support/status.hpp"
#include "proto/export/export_answer.hpp"
#include "proto/export/export_request.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class FakeExportConsumer {
public:
    FakeExportConsumer();
    ~FakeExportConsumer();
    FakeExportConsumer(const FakeExportConsumer &) = delete;
    FakeExportConsumer &operator=(const FakeExportConsumer &) = delete;

    /* The device's end, to accept; the consumer stops owning it once it is
     * handed over, and owns it again if the device refuses it. */
    int  device_end();
    void refused();

    /* One request as encoded, or `length` raw bytes: whether it was sent. */
    bool send(const ExportRequest &request);
    bool send_bytes(const std::uint8_t *bytes, std::size_t length);
    /* The next answer within `milliseconds`, decoded: ok, the decoder's
     * refusal, invalid_state when nothing came, not_found once the device
     * closed its end. Its descriptor, if one came, is kept as descriptor(). */
    Status receive(ExportAnswer &answer, int milliseconds = 5000);
    /* The descriptor the last answer brought, or -1; the consumer's until the
     * next receive or its end. */
    int    descriptor() const;
    /* Whether the device has shut its end: a read finds the stream ended. */
    bool   hung_up(int milliseconds = 5000);
    /* The consumer goes, as a process that exits. */
    void   leave();

private:
    void forget_descriptor();

    int consumer_;
    /* The device's end before it is handed over, and while the device has
     * it - so a refusal gives it back. */
    int device_;
    int held_;
    int descriptor_;
};

}  // namespace av

#endif  // AV_FAKE_EXPORT_CONSUMER_HPP
