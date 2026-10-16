/* Commands a stream over a transport holds back to send as one batch, with
 * the blob leases their arrays were copied into.
 *
 * The transport takes every lease taken so far to be carried by the batch it
 * publishes next (ClientBlobArena::mark), and every lease to be read once a
 * call is answered (retire_all). A command of the transport's own - a blob
 * import, a no-op - sent while a stream still holds commands back breaks
 * both: their leases were retired, lent again and written over before the
 * stream's batch reached the host, which then read another array where the
 * one it was sent should have been. Resident Evil 4 made upload heaps (a blob
 * import each) between the 70-85 KB compute shaders it built, and the host
 * refused the shaders it was handed, and removed the device.
 *
 * So a transport told of a stream (KmtTransport::send_after) has it send what
 * it holds before a command of the transport's own goes, which also keeps
 * those commands in the order the application made them. Two implementations:
 * the Direct3D command stream's, and the tests'. */
#ifndef AV_GUEST_UNSENT_BATCH_HPP
#define AV_GUEST_UNSENT_BATCH_HPP

#include "core/support/status.hpp"

namespace av {

class UnsentBatch {
public:
    UnsentBatch(const UnsentBatch &) = delete;
    UnsentBatch &operator=(const UnsentBatch &) = delete;

    /* Publishes what is held back as a batch on the transport, answered by
     * nothing; ok with nothing held. A failure fails the stream, and the
     * transport's command is not sent. */
    [[nodiscard]] virtual Status flush() = 0;

protected:
    UnsentBatch() = default;
    ~UnsentBatch() = default;
};

}  // namespace av

#endif  // AV_GUEST_UNSENT_BATCH_HPP
